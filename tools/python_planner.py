# -*- coding: utf-8 -*-

import socket
import json
import math


# =========================================
# V1.2 简化 Lattice Planner
# =========================================

HOST = "127.0.0.1"
PORT = 8080


# =========================================
# 5个候选横向偏移
# =========================================
#
# 负数：道路一侧
# 0：道路中心
# 正数：道路另一侧
#
CANDIDATE_OFFSETS = [-3.5, -1.75, 0.0, 1.75, 3.5]


# =========================================
# 规划参数
# =========================================

# 只考虑车辆前方35米以内的障碍物
MAX_LOOKAHEAD = 35.0

# 候选横向位置距离障碍物小于该值时，
# 认为碰撞风险很高
SAFE_LATERAL_DISTANCE = 1.2


# =========================================
# 三种代价权重
# =========================================

# 偏离道路中心的代价
W_LANE = 2.0

# 与上一帧规划结果变化过大的代价
W_SMOOTH = 3.0

# 碰撞风险代价
W_COLLISION = 1000.0


def get_forward_obstacles(obstacles):
    """
    从所有检测到的障碍物中，
    筛选出真正位于车辆前方规划范围内的障碍物。
    """

    forward_obstacles = []

    for obstacle in obstacles:
        x = float(obstacle.get("local_x", 0.0))

        # 只保留车辆前方0~35米范围内的障碍物
        if 0.0 < x <= MAX_LOOKAHEAD:
            forward_obstacles.append(obstacle)

    return forward_obstacles


def evaluate_lattice_cost(target_offset, obstacles, last_offset):
    """
    计算一条候选横向轨迹的总代价。

    总代价 =
        偏离道路中心代价
        + 平滑代价
        + 障碍物碰撞代价
    """

    cost = 0.0

    # =====================================
    # 1. 偏离道路中心代价
    # =====================================
    #
    # offset越大，说明离y=0越远。
    #
    # 没有障碍物时，算法应该更倾向于：
    #
    # target_offset = 0
    #
    cost += W_LANE * abs(target_offset)


    # =====================================
    # 2. 平滑代价
    # =====================================
    #
    # 防止规划结果：
    #
    # +1.75
    # ↓
    # -1.75
    # ↓
    # +1.75
    #
    # 来回跳动。
    #
    cost += W_SMOOTH * abs(target_offset - last_offset)


    # =====================================
    # 3. 障碍物碰撞代价
    # =====================================

    for obstacle in obstacles:
        x = float(obstacle.get("local_x", 0.0))
        y = float(obstacle.get("local_y", 0.0))

        # 防御性判断：
        # 后方障碍物不参与当前规划
        if x <= 0.0:
            continue

        # 规划范围以外暂时不参与
        if x > MAX_LOOKAHEAD:
            continue


        # =================================
        # 计算候选轨迹与障碍物的横向距离
        # =================================

        lateral_gap = abs(y - target_offset)


        # =================================
        # 距离太近：巨大碰撞代价
        # =================================

        if lateral_gap < SAFE_LATERAL_DISTANCE:
            cost += W_COLLISION * (SAFE_LATERAL_DISTANCE - lateral_gap + 1.0)

        else:
            # 没有直接碰撞，
            # 但离障碍物越近仍然给予一定惩罚
            cost += 5.0 / (lateral_gap + 0.1)

    return cost


def choose_best_offset(obstacles, last_offset):
    """
    根据当前障碍物选择最佳横向偏移。

    V1.2最重要的变化：

    前方没有障碍物：
        直接返回0
        让车辆重新回到道路中心

    前方存在障碍物：
        才进行5条Lattice候选轨迹的代价比较
    """

    # =====================================
    # 1. 筛选前方障碍物
    # =====================================

    forward_obstacles = get_forward_obstacles(obstacles)

    print("前方规划范围内障碍物数量:", len(forward_obstacles))


    # =====================================
    # 2. 前方已经没有障碍物
    # =====================================
    #
    # 这是V1.2解决“避障后不回中”的关键。
    #
    if not forward_obstacles:
        print("前方无障碍物，规划目标回到道路中心：0.0")
        return 0.0


    # =====================================
    # 3. 有障碍物时执行Lattice规划
    # =====================================

    best_offset = 0.0
    best_cost = float("inf")


    for offset in CANDIDATE_OFFSETS:
        cost = evaluate_lattice_cost(offset, forward_obstacles, last_offset)

        print("offset={:5.2f}, cost={:8.2f}".format(offset, cost))

        if cost < best_cost:
            best_cost = cost
            best_offset = offset


    print("最小代价:", best_cost)

    return best_offset

def validate_request(message):
    if not isinstance(message, dict):
        raise ValueError("root must be object")
    if type(message.get("protocol_version")) is not int or message["protocol_version"] != 1:
        raise ValueError("unsupported protocol_version")
    if message.get("type") != "OBSTACLES":
        raise ValueError("unsupported type")
    session = message.get("session_id")
    frame = message.get("frame_id")
    if not isinstance(session, str) or not (1 <= len(session) <= 64):
        raise ValueError("invalid session_id")
    if type(frame) is not int or not (1 <= frame <= 2147483647):
        raise ValueError("invalid frame_id")
    obstacles = message.get("data")
    if not isinstance(obstacles, list):
        raise ValueError("data must be array")
    for item in obstacles:
        if not isinstance(item, dict):
            raise ValueError("obstacle must be object")
        for key in ("local_x", "local_y", "dist"):
            value = item.get(key)
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError("invalid obstacle field: " + key)
    return session, frame, obstacles

def main():
    # =====================================
    # 创建TCP客户端
    # =====================================

    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    print("正在连接 ADASim...")

    try:
        client.connect((HOST, PORT))
    except ConnectionRefusedError:
        print("连接失败：请先启动 ADASim C++ 程序")
        client.close()
        return

    print("已连接 ADASim TCP Server")


    # =====================================
    # TCP接收缓存
    # =====================================

    buffer = ""


    # =====================================
    # 上一帧规划结果
    # =====================================
    #
    # 用于计算平滑代价。
    #
    last_offset = 0.0
    last_session = None


    # =====================================
    # 主循环
    # =====================================

    while True:
        try:
            data = client.recv(4096)
        except ConnectionResetError:
            print("ADASim连接被关闭")
            break


        # C++关闭连接
        if not data:
            print("ADASim连接已关闭")
            break


        buffer += data.decode("utf-8")


        # =================================
        # 我们规定：
        #
        # 每一行就是一条完整JSON消息
        # =================================

        while "\n" in buffer:
            line, buffer = buffer.split("\n", 1)


            if not line.strip():
                continue


            # =================================
            # JSON解析
            # =================================

            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                print("收到非法JSON：", line)
                continue


            # =================================
            # 只处理OBSTACLES消息
            # =================================

            try:
                session, frame, obstacles = validate_request(message)
            except (ValueError, OverflowError) as exc:
                print("[PROTOCOL]", exc, flush=True)
            continue

            if session != last_session:
                last_offset = 0.0
                last_session = session


            print("\n=====================================")
            print("收到障碍物数量:", len(obstacles))


            # =================================
            # Lattice规划
            # =================================

            best_offset = choose_best_offset(obstacles, last_offset)


            print("选择轨迹:", best_offset)


            # 保存这一帧的规划结果，
            # 下一帧用于平滑代价计算
            last_offset = best_offset


            # =================================
            # 把规划结果发送回C++
            # =================================

            response = {
                "type": "CONTROL",
                "steer_offset": best_offset
                "protocol_version": 1,
                "session_id": session,
                "frame_id": frame,
            }


            response_data = json.dumps(response, allow_nan=False)(response) + "\n"

            client.sendall(response_data.encode("utf-8"))


    # =====================================
    # 关闭连接
    # =====================================

    client.close()

    print("Python Planner 已退出")


if __name__ == "__main__":
    main()