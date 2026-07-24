import re
from datetime import datetime
from pathlib import Path

VERSION_PATH = Path(__file__).resolve().parents[1] / "include" / "version.h"


def main() -> int:
    text = VERSION_PATH.read_text(encoding="ascii")

    m = re.search(r"#define\s+EAGLEOS_BUILD_NUMBER\s+(\d+)", text)
    if not m:
        print("EAGLEOS_BUILD_NUMBER not found")
        return 1

    revision_match = re.search(r"#define\s+EAGLEOS_BUILD_REVISION\s+(\d+)", text)
    if not revision_match:
        print("EAGLEOS_BUILD_REVISION not found")
        return 1

    build = int(m.group(1)) + 1
    revision = int(revision_match.group(1))
    now = datetime.now()
    build_date = now.strftime("%m%d%y")
    build_time = now.strftime("%H%M")
    tag = f"{build}.{revision}.{build_date}.{build_time}"

    text = re.sub(
        r"#define\s+EAGLEOS_BUILD_NUMBER\s+\d+",
        f"#define EAGLEOS_BUILD_NUMBER {build}",
        text,
        count=1,
    )
    text = re.sub(
        r"#define\s+EAGLEOS_BUILD_DATE\s+\"[^\"]*\"",
        f"#define EAGLEOS_BUILD_DATE \"{build_date}\"",
        text,
        count=1,
    )
    text = re.sub(
        r"#define\s+EAGLEOS_BUILD_TIME\s+\"[^\"]*\"",
        f"#define EAGLEOS_BUILD_TIME \"{build_time}\"",
        text,
        count=1,
    )
    text = re.sub(
        r"#define\s+EAGLEOS_BUILD_TAG\s+\"[^\"]*\"",
        f"#define EAGLEOS_BUILD_TAG \"{tag}\"",
        text,
        count=1,
    )

    VERSION_PATH.write_text(text, encoding="ascii", newline="\n")
    print(f"Build tag updated to {tag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
