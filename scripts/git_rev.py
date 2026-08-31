"""PlatformIO pre-build hook: SFERIC_BUILD_GIT = git describe (short hash + -dirty)."""
import subprocess

Import("env")  # noqa: F821


def rev() -> str:
    try:
        out = subprocess.check_output(
            ["git", "describe", "--always", "--dirty", "--abbrev=8"],
            stderr=subprocess.DEVNULL,
        )
        return out.decode().strip() or "unknown"
    except Exception:
        return "unknown"


env.Append(CPPDEFINES=[("SFERIC_BUILD_GIT", env.StringifyMacro(rev()))])
print("SFERIC_BUILD_GIT =", rev())
