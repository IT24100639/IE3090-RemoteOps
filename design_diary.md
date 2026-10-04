# Design Diary - IT24100639

## 2 October 2026 - Preparation
Used my existing Windows laptop and CentOS 10 VirtualBox VM.
SSH initially failed because the VM had no IPv4 address. Changing
to NAT and forwarding host port 2222 to guest port 22 restored access.
Checked GCC and Make, installed Git, and prepared the repository.
Personalised settings are TCP port 9410, SID:9360 and token OPS-0639.

## 3 October 2026 - TCP and concurrency
Implemented the Agent and Controller in C using BSD sockets.
Selected one detached pthread per client so a waiting client does
not block other sessions. Each session owns its authentication state.
Five C Controllers connected simultaneously, authenticated and
received BYE responses. Detached threads release resources on exit.

Added SYSINFO using sysinfo() and LISTPROC using /proc/<pid>/comm.
SYSINFO reports load average, used RAM and uptime; load is not a
percentage. LISTPROC limits output to 20 readable processes and
sanitises names to keep the response within one protocol line.
Both commands were rejected before authentication and worked afterward.

## 4 October 2026 - Commands, transfers and monitoring
Added exact EXEC handlers for DATE, UPTIME, DISKFREE, HOSTNAME and
WHOAMI using C APIs. Avoiding a shell prevents command strings from
being executed. Unknown commands, extra arguments and semicolons
were rejected.

PUT and GET use newline headers followed by exact-length binary data.
Send and receive loops handle partial socket operations. Selected
4096-byte transfer chunks and a 100 MiB limit. Restricted filenames
prevent path traversal. Temporary files are renamed after completion;
interrupted uploads remove temporary files. Rejected PUT requests
close the session to avoid interpreting unread payload as commands.

Added a monitoring worker per session, sending UDP statistics every
two seconds to the TCP peer's requested port. A condition variable
allows monitoring to stop promptly. Added a Makefile and mutex-protected
timestamped logs for connections, commands and transfers. AUTH tokens
are redacted.

## 4 October 2026 - Validation
Text, 65,536-byte binary and zero-byte transfers passed. Binary
SHA-256 hashes matched across original, stored and downloaded files.
Invalid paths and missing files returned errors. Direct Python socket
tests confirmed oversized PUT rejection, interrupted-upload cleanup,
split commands, combined commands and binary payload boundaries.

Five connected test clients answered SYSINFO. Two monitoring sessions
operated independently. STOP, QUIT and TCP disconnect stopped the
affected session's UDP updates while other clients continued working.
Screenshots 01-19 record feature and test evidence.

## 5 October 2026 - Documentation review
Updated the README with build, run, protocol, testing and limitation
details. Retained the longer diary in docs/development_history.md.
ChatGPT/Codex supplied substantial code, testing and documentation
assistance, recorded in prompt_log.md. I ran the builds and tests
in CentOS and captured the observed results. Report preparation
and final packaging remain to be completed.
