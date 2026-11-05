"""Decode the real OLED GPIO output for the dial test screens."""
import shutil
import tempfile
from pathlib import Path

from test_pin_migration import ROOT, STUB
from test_pattern_display import DISPLAY_TEST, EXTRA
from test_pattern_lock import run_target

TEST = DISPLAY_TEST.split("int main(void) {")[0] + r"""
#include "dial_sequence.h"
#include "dial_view.h"
int main(void) {
    DialSequence s;
    unsigned i;
    OLED_Init();
    PatternView_Message("INPUT TEST", "1 Dial test", "2 Password test", "3Duress 4Logs");
    image("basic/build/dial-menu.pbm");
    DialSequence_Clear(&s);
    DialView_Show(&s, "# OK");
    assert(pixel(28, 6) && pixel(28, 24));
    image("basic/build/dial-empty.pbm");
    for (i = 0; i < 3; i++) DialSequence_Append(&s, 1);
    DialView_Show(&s, "# OK");
    assert(pixel(44, 24) && !pixel(28, 12)); /* quarter turn points to the right */
    image("basic/build/dial-r3.pbm");
    for (i = 0; i < 2; i++) DialSequence_Append(&s, -1);
    for (i = 0; i < 4; i++) DialSequence_Append(&s, 1);
    DialView_Show(&s, "# OK");
    image("basic/build/dial-ready.pbm");
    DialView_Show(&s, "PASS");
    image("basic/build/dial-pass.pbm");
    DialView_Show(&s, "NO MATCH");
    image("basic/build/dial-fail.pbm");
    puts("PASS: real OLED dial frames, clockwise pointer, three segment counters and result labels");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-dial-display-", dir=build_root) as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", EXTRA + "\n#endif"))
        run_target(temp, "dial_display", TEST,
                   ["hardware/OLED.c", "hardware/pattern_view.c", "hardware/pattern.c",
                    "hardware/dial_sequence.c", "hardware/dial_view.c"], ["-Wno-missing-braces"])


if __name__ == "__main__":
    main()
