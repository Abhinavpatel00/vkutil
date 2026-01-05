
#!/usr/bin/env bash
set -e

echo "[*] Building project. Try not to panic."

# collect sources and existing prebuilt objects
SRC_FILES=$(ls *.c)
OBJ_FILES=$(ls *.o 2>/dev/null || true)

# tweak flags as needed
CFLAGS="-std=c99"
LIBS="-lm -lglfw"

gcc $CFLAGS $SRC_FILES $OBJ_FI LES -o test $LIBS

echo "[+] Done. Executable: ./test"







