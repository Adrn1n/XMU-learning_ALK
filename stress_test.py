import argparse
import datetime
import os
import random
import re
import sqlite3
import subprocess
import sys
import tempfile
import time
from typing import List, Tuple, Dict, Optional

DATE_BEGIN = datetime.date(2026, 1, 1)
DATE_END_EXCLUSIVE = datetime.date(2028, 1, 1)
TOTAL_DAYS = (DATE_END_EXCLUSIVE - DATE_BEGIN).days


def day_to_date(day_index: int) -> datetime.date:
    return DATE_BEGIN + datetime.timedelta(days=day_index)


def date_to_day(d: datetime.date) -> int:
    return (d - DATE_BEGIN).days


def parse_date(s: str) -> int:
    d = datetime.datetime.strptime(s, "%Y-%m-%d").date()
    return date_to_day(d)


def random_date_and_duration(
    max_start_day: int, max_duration: int
) -> Tuple[int, int, int, int]:
    """
    返回 year, month, day, duration。
    为避免越界，确保 start + duration <= TOTAL_DAYS。
    """
    start = random.randint(0, max_start_day)
    max_allowed = min(max_duration, TOTAL_DAYS - start)
    duration = random.randint(1, max_allowed)
    d = day_to_date(start)
    return d.year, d.month, d.day, duration


def make_device_ids(device_count: int, sparse: bool) -> List[int]:
    """
    生成设备编号。
    sparse=True 时生成非连续设备编号，用于测试 invalid device 和 block 逻辑。
    """
    if not sparse:
        return list(range(1, device_count + 1))

    pool = list(range(1, device_count * 4 + 20))
    random.shuffle(pool)
    ids = sorted(pool[:device_count])
    return ids


def make_invalid_device_id(valid_ids: List[int]) -> int:
    s = set(valid_ids)
    x = max(valid_ids) + random.randint(1, 20)
    while x in s:
        x += 1
    return x


def choose_operation() -> str:
    """
    操作比例。
    reserve 类多一点，cancel 类次之，check 少量。
    """
    ops = [
        ("reserve", 25),
        ("reserveblock", 12),
        ("reserveany", 15),
        ("cancel", 18),
        ("cancelblock", 8),
        ("cancelany", 10),
        ("check", 12),
    ]
    total = sum(w for _, w in ops)
    r = random.randint(1, total)
    acc = 0
    for op, w in ops:
        acc += w
        if r <= acc:
            return op
    return "check"


def generate_random_input(
    device_count: int,
    user_count: int,
    ops_per_user: int,
    sparse_devices: bool,
    delay_mode: str,
    invalid_rate: float,
    max_duration: int,
    max_start_day: int,
) -> Tuple[str, List[int]]:
    """
    生成实验输入文件内容。
    """
    device_ids = make_device_ids(device_count, sparse_devices)
    users = [f"User{i}" for i in range(user_count)]

    lines = []
    lines.append(str(device_count))
    lines.append(" ".join(str(x) for x in device_ids))
    lines.append(str(user_count))

    for u in users:
        lines.append("user")

        if delay_mode == "zero":
            reserve_delay = 0
            cancel_delay = 0
            check_delay = 0
        elif delay_mode == "light":
            # 注意：这里最多偶尔给 1 秒。
            # 因为 sleep 在临界区里，过多 1 秒会导致压测非常慢。
            reserve_delay = 1 if random.random() < 0.08 else 0
            cancel_delay = 1 if random.random() < 0.05 else 0
            check_delay = 1 if random.random() < 0.03 else 0
        else:
            reserve_delay = 0
            cancel_delay = 0
            check_delay = 0

        lines.append(f"reserve {reserve_delay}")
        lines.append(f"cancel {cancel_delay}")
        lines.append(f"check {check_delay}")

        for _ in range(ops_per_user):
            op = choose_operation()

            if op == "check":
                lines.append(f"check {u}")
                continue

            y, m, d, duration = random_date_and_duration(max_start_day, max_duration)

            use_invalid = random.random() < invalid_rate

            if op == "reserve":
                if use_invalid:
                    dev = make_invalid_device_id(device_ids)
                else:
                    dev = random.choice(device_ids)
                lines.append(f"reserve {dev} {y} {m} {d} {duration} {u}")

            elif op == "reserveblock":
                count = random.randint(1, min(5, device_count))
                if use_invalid:
                    first = make_invalid_device_id(device_ids)
                else:
                    first = random.choice(device_ids)
                lines.append(f"reserveblock {count} {first} {y} {m} {d} {duration} {u}")

            elif op == "reserveany":
                if use_invalid:
                    count = device_count + random.randint(1, 5)
                else:
                    count = random.randint(1, min(5, device_count))
                lines.append(f"reserveany {count} {y} {m} {d} {duration} {u}")

            elif op == "cancel":
                if use_invalid:
                    dev = make_invalid_device_id(device_ids)
                else:
                    dev = random.choice(device_ids)
                lines.append(f"cancel {dev} {y} {m} {d} {duration} {u}")

            elif op == "cancelblock":
                count = random.randint(1, min(5, device_count))
                if use_invalid:
                    first = make_invalid_device_id(device_ids)
                else:
                    first = random.choice(device_ids)
                lines.append(f"cancelblock {count} {first} {y} {m} {d} {duration} {u}")

            elif op == "cancelany":
                if use_invalid:
                    count = device_count + random.randint(1, 5)
                else:
                    count = random.randint(1, min(5, device_count))
                lines.append(f"cancelany {count} {y} {m} {d} {duration} {u}")

        lines.append("end.")

    return "\n".join(lines) + "\n", device_ids


FINAL_TABLE_BEGIN = "========== Final Reservation Table =========="
FINAL_TABLE_END = "============================================="

DEVICE_LINE_RE = re.compile(r"^Device\s+(-?\d+):\s*$")
RES_LINE_RE = re.compile(
    r"^\s*user=([A-Za-z0-9_\-\.]+)\s+start=(\d{4}-\d{2}-\d{2})\s+end=(\d{4}-\d{2}-\d{2})\s*$"
)


def parse_final_table(output: str) -> List[Tuple[int, str, int, int]]:
    """
    解析最终预约表。

    返回:
        [(device_id, user_name, start_day, end_day), ...]
    """
    lines = output.splitlines()

    in_table = False
    current_device = None
    reservations = []

    for line in lines:
        line = line.rstrip("\n")

        if FINAL_TABLE_BEGIN in line:
            in_table = True
            continue

        if FINAL_TABLE_END in line:
            in_table = False
            break

        if not in_table:
            continue

        m_dev = DEVICE_LINE_RE.match(line)
        if m_dev:
            current_device = int(m_dev.group(1))
            continue

        if line.strip() == "empty" or line.strip() == "No reservations.":
            continue

        m_res = RES_LINE_RE.match(line)
        if m_res:
            if current_device is None:
                raise ValueError(f"reservation line without device line: {line}")

            user = m_res.group(1)
            start = parse_date(m_res.group(2))
            end = parse_date(m_res.group(3))
            reservations.append((current_device, user, start, end))
            continue

    return reservations


def validate_with_sqlite(
    reservations: List[Tuple[int, str, int, int]],
    valid_device_ids: List[int],
) -> List[str]:
    """
    用 sqlite3 做最终状态一致性检查。
    """
    errors = []

    conn = sqlite3.connect(":memory:")
    cur = conn.cursor()

    cur.execute("""
        CREATE TABLE valid_device (
            device_id INTEGER PRIMARY KEY
        )
    """)

    cur.execute("""
        CREATE TABLE reservation (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id INTEGER NOT NULL,
            user_name TEXT NOT NULL,
            start_day INTEGER NOT NULL,
            end_day INTEGER NOT NULL
        )
    """)

    cur.executemany(
        "INSERT INTO valid_device(device_id) VALUES (?)",
        [(x,) for x in valid_device_ids],
    )

    cur.executemany(
        """
        INSERT INTO reservation(device_id, user_name, start_day, end_day)
        VALUES (?, ?, ?, ?)
        """,
        reservations,
    )

    conn.commit()

    # 1. 检查设备是否合法
    cur.execute("""
        SELECT r.device_id
        FROM reservation r
        LEFT JOIN valid_device d ON r.device_id = d.device_id
        WHERE d.device_id IS NULL
        LIMIT 10
    """)
    bad_devices = cur.fetchall()
    if bad_devices:
        errors.append(f"存在非法设备预约: {bad_devices[:5]}")

    # 2. 检查日期范围
    cur.execute(
        """
        SELECT id, device_id, user_name, start_day, end_day
        FROM reservation
        WHERE start_day < 0
           OR end_day > ?
           OR start_day >= end_day
        LIMIT 10
    """,
        (TOTAL_DAYS,),
    )
    bad_dates = cur.fetchall()
    if bad_dates:
        errors.append(f"存在非法日期区间: {bad_dates[:5]}")

    # 3. 检查同一设备上的重叠预约
    # 左闭右开区间重叠条件:
    # a.start < b.end AND b.start < a.end
    cur.execute("""
        SELECT
            a.id, b.id,
            a.device_id,
            a.user_name, a.start_day, a.end_day,
            b.user_name, b.start_day, b.end_day
        FROM reservation a
        JOIN reservation b
          ON a.device_id = b.device_id
         AND a.id < b.id
         AND a.start_day < b.end_day
         AND b.start_day < a.end_day
        LIMIT 10
    """)
    overlaps = cur.fetchall()
    if overlaps:
        errors.append(f"存在同一设备时间重叠预约: {overlaps[:3]}")

    # 4. 检查空用户名
    cur.execute("""
        SELECT id, device_id, user_name
        FROM reservation
        WHERE user_name IS NULL OR LENGTH(user_name) = 0
        LIMIT 10
    """)
    bad_users = cur.fetchall()
    if bad_users:
        errors.append(f"存在非法用户名: {bad_users[:5]}")

    conn.close()
    return errors


def run_one_case(
    exe: str,
    case_text: str,
    device_ids: List[int],
    timeout_sec: float,
    keep_dir: Optional[str],
    case_id: int,
    verbose: bool,
) -> bool:
    """
    运行一个测试用例。
    返回 True 表示通过。
    """
    input_path = None

    try:
        if keep_dir:
            os.makedirs(keep_dir, exist_ok=True)
            input_path = os.path.join(keep_dir, f"case_{case_id}.txt")
            with open(input_path, "w", encoding="utf-8") as f:
                f.write(case_text)
        else:
            tmp = tempfile.NamedTemporaryFile(
                mode="w",
                suffix=".txt",
                prefix=f"lab_case_{case_id}_",
                delete=False,
                encoding="utf-8",
            )
            input_path = tmp.name
            tmp.write(case_text)
            tmp.close()

        start_time = time.time()

        proc = subprocess.run(
            [exe, input_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_sec,
        )

        elapsed = time.time() - start_time

        if proc.returncode != 0:
            print(
                f"\n[FAIL] case {case_id}: 程序返回非 0，returncode={proc.returncode}"
            )
            print("----- stderr -----")
            print(proc.stderr)
            print("----- stdout -----")
            print(proc.stdout[-5000:])
            if keep_dir:
                print(f"输入文件已保存: {input_path}")
            return False

        if FINAL_TABLE_BEGIN not in proc.stdout or FINAL_TABLE_END not in proc.stdout:
            print(f"\n[FAIL] case {case_id}: 未找到最终预约表")
            print("----- stdout tail -----")
            print(proc.stdout[-5000:])
            if keep_dir:
                print(f"输入文件已保存: {input_path}")
            return False

        try:
            reservations = parse_final_table(proc.stdout)
        except Exception as e:
            print(f"\n[FAIL] case {case_id}: 解析最终预约表失败: {e}")
            print("----- stdout tail -----")
            print(proc.stdout[-5000:])
            if keep_dir:
                print(f"输入文件已保存: {input_path}")
            return False

        errors = validate_with_sqlite(reservations, device_ids)
        if errors:
            print(f"\n[FAIL] case {case_id}: SQLite 一致性检查失败")
            for err in errors:
                print("  -", err)

            print("----- input -----")
            print(case_text[:8000])
            if len(case_text) > 8000:
                print("... input truncated ...")

            print("----- stdout tail -----")
            print(proc.stdout[-8000:])

            if keep_dir:
                out_path = os.path.join(keep_dir, f"case_{case_id}.out")
                err_path = os.path.join(keep_dir, f"case_{case_id}.err")
                with open(out_path, "w", encoding="utf-8") as f:
                    f.write(proc.stdout)
                with open(err_path, "w", encoding="utf-8") as f:
                    f.write(proc.stderr)
                print(f"输入文件已保存: {input_path}")
                print(f"输出文件已保存: {out_path}")
                print(f"错误文件已保存: {err_path}")

            return False

        if verbose:
            print(
                f"[PASS] case {case_id}: "
                f"reservations={len(reservations)}, "
                f"time={elapsed:.3f}s"
            )

        return True

    except subprocess.TimeoutExpired:
        print(
            f"\n[FAIL] case {case_id}: 超时，可能死锁或运行过慢，timeout={timeout_sec}s"
        )
        if keep_dir and input_path:
            print(f"输入文件已保存: {input_path}")
        return False

    finally:
        if input_path and not keep_dir:
            try:
                os.unlink(input_path)
            except OSError:
                pass


def generate_targeted_case() -> Tuple[str, List[int]]:
    """
    生成一个固定针对性用例，主要覆盖：
    - reserve 冲突
    - 部分 cancel 拆分
    - reserveblock 环形连续
    - reserveany
    - cancelany
    """
    device_ids = [1, 2, 3, 4, 5]

    text = """5
1 2 3 4 5
3
user
reserve 0
cancel 0
check 0
reserve 1 2026 1 1 5 Alice
check Alice
cancel 1 2026 1 2 2 Alice
check Alice
end.
user
reserve 0
cancel 0
check 0
reserve 1 2026 1 3 3 Bob
reserveany 2 2026 1 1 3 Bob
check Bob
end.
user
reserve 0
cancel 0
check 0
reserveblock 3 4 2026 2 1 2 Carol
check Carol
cancelany 1 2026 2 1 2 Carol
check Carol
end.
"""
    return text, device_ids


def main():
    parser = argparse.ArgumentParser(
        description="Linux 设备预约系统压测脚本，无第三方依赖，使用 sqlite3 检查最终状态一致性。"
    )

    parser.add_argument(
        "exe", help="lab_reservation 可执行文件路径，例如 ./build/lab_reservation"
    )

    parser.add_argument(
        "-n", "--cases", type=int, default=100, help="随机测试用例数量，默认 100"
    )

    parser.add_argument(
        "--users", type=int, default=8, help="每个随机用例的用户数，默认 8"
    )

    parser.add_argument(
        "--devices", type=int, default=16, help="每个随机用例的设备数，默认 16"
    )

    parser.add_argument("--ops", type=int, default=40, help="每个用户的操作数，默认 40")

    parser.add_argument(
        "--timeout",
        type=float,
        default=8.0,
        help="每个用例的超时时间，默认 8 秒。若启用 light delay，可适当增大。",
    )

    parser.add_argument("--seed", type=int, default=None, help="随机**，指定后可复现")

    parser.add_argument(
        "--delay-mode",
        choices=["zero", "light"],
        default="zero",
        help="sleep 延迟模式。zero 表示全 0，light 表示少量用户操作延迟为 1 秒。默认 zero。",
    )

    parser.add_argument(
        "--sparse-devices",
        action="store_true",
        help="生成非连续设备编号，用于测试 invalid device 和 block 边界",
    )

    parser.add_argument(
        "--invalid-rate",
        type=float,
        default=0.03,
        help="生成非法设备或非法 count 的概率，默认 0.03",
    )

    parser.add_argument(
        "--max-duration", type=int, default=10, help="随机预约最大 duration，默认 10"
    )

    parser.add_argument(
        "--max-start-day",
        type=int,
        default=120,
        help="随机开始日期最大 day index，默认 120，即主要集中在 2026 年前几个月",
    )

    parser.add_argument(
        "--keep-failed", default=None, help="保存失败用例的目录，例如 ./failed_cases"
    )

    parser.add_argument(
        "--keep-all", default=None, help="保存所有用例的目录，例如 ./all_cases"
    )

    parser.add_argument(
        "--no-targeted", action="store_true", help="不运行固定针对性用例"
    )

    parser.add_argument(
        "-v", "--verbose", action="store_true", help="输出每个 case 的通过信息"
    )

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
        print(f"[INFO] seed = {args.seed}")
    else:
        seed = int(time.time() * 1000) & 0xFFFFFFFF
        random.seed(seed)
        print(f"[INFO] seed = {seed}")

    if not os.path.exists(args.exe):
        print(f"[ERROR] executable not found: {args.exe}", file=sys.stderr)
        return 2

    total = 0
    passed = 0

    # 先跑一个针对性用例
    if not args.no_targeted:
        total += 1
        case_text, device_ids = generate_targeted_case()
        keep_dir = args.keep_all or args.keep_failed
        ok = run_one_case(
            exe=args.exe,
            case_text=case_text,
            device_ids=device_ids,
            timeout_sec=args.timeout,
            keep_dir=keep_dir if args.keep_all else None,
            case_id=0,
            verbose=args.verbose,
        )
        if ok:
            passed += 1
        else:
            if args.keep_failed and not args.keep_all:
                os.makedirs(args.keep_failed, exist_ok=True)
                path = os.path.join(args.keep_failed, "targeted_case_0.txt")
                with open(path, "w", encoding="utf-8") as f:
                    f.write(case_text)
                print(f"失败针对性用例已保存: {path}")
            print(f"[SUMMARY] passed {passed}/{total}")
            return 1

    for i in range(1, args.cases + 1):
        total += 1

        case_text, device_ids = generate_random_input(
            device_count=args.devices,
            user_count=args.users,
            ops_per_user=args.ops,
            sparse_devices=args.sparse_devices,
            delay_mode=args.delay_mode,
            invalid_rate=args.invalid_rate,
            max_duration=args.max_duration,
            max_start_day=args.max_start_day,
        )

        if args.keep_all:
            keep_dir = args.keep_all
        else:
            keep_dir = None

        ok = run_one_case(
            exe=args.exe,
            case_text=case_text,
            device_ids=device_ids,
            timeout_sec=args.timeout,
            keep_dir=keep_dir,
            case_id=i,
            verbose=args.verbose,
        )

        if ok:
            passed += 1
        else:
            if args.keep_failed and not args.keep_all:
                os.makedirs(args.keep_failed, exist_ok=True)
                path = os.path.join(args.keep_failed, f"case_{i}.txt")
                with open(path, "w", encoding="utf-8") as f:
                    f.write(case_text)
                print(f"失败用例已保存: {path}")

            print(f"[SUMMARY] passed {passed}/{total}")
            return 1

        if not args.verbose and i % 10 == 0:
            print(f"[INFO] passed random cases: {i}/{args.cases}")

    print(f"[SUMMARY] all passed: {passed}/{total}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
