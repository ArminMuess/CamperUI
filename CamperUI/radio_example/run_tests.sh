#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
RADIO_TEST_DIR=$(mktemp -d)
trap 'rm -rf "$RADIO_TEST_DIR"' EXIT
for test in test_radio test_config; do
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic "radio_example/$test.cpp" -o "$RADIO_TEST_DIR/$test"
  "$RADIO_TEST_DIR/$test"
done
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iradio_example/host_stubs radio_config.cpp radio_example/test_persistence.cpp -o "$RADIO_TEST_DIR/persistence"
"$RADIO_TEST_DIR/persistence"
ARDUINOJSON_SRC=${ARDUINOJSON_SRC:-$HOME/tc/ArduinoJson/src}
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I"$ARDUINOJSON_SRC" radio_example/test_powerunit.cpp -o "$RADIO_TEST_DIR/powerunit"
"$RADIO_TEST_DIR/powerunit"
python3 radio_example/test_probe.py
python3 - "$RADIO_TEST_DIR/settings.js" <<'PY'
import re,sys
from pathlib import Path
for i,name in enumerate(['radio_settings.cpp','powerunit_settings.cpp']):
    s=Path(name).read_text()
    html=re.search(r'R"HTML\((.*?)\)HTML"',s,re.S).group(1)
    Path(sys.argv[1]+str(i)+".js").write_text(re.search(r'<script>(.*?)</script>',html,re.S).group(1))
PY
node --check "$RADIO_TEST_DIR/settings.js0.js"
node --check "$RADIO_TEST_DIR/settings.js1.js"
git diff --check
