set pagination off
set confirm off
set print thread-events off
set logging file /tmp/radiant_crash_bt.txt
set logging overwrite on
set logging enabled on
handle SIGPIPE nostop noprint pass

run

echo \n=== CRASH BACKTRACE ===\n
bt full
echo \n=== THREADS ===\n
info threads
echo \n=== ALL THREAD BACKTRACES ===\n
thread apply all bt full

quit
