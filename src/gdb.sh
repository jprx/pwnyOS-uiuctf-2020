
if [[ -z $DEBUG_HOST ]]; then
    DEBUG_HOST="localhost:1234"
fi

gdb kernel -ex "set confirm off" -ex "target remote $DEBUG_HOST"
