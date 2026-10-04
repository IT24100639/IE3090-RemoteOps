# Design Diary

## 2 October 2026 - Environment and project preparation

I am using a Windows laptop with CentOS 10 in Oracle VirtualBox.
I access CentOS through SSH from Windows Terminal.

Initially, SSH did not connect. The SSH service was running,
but the CentOS network interface had no IPv4 address.
I changed the VirtualBox network from Bridged Adapter to NAT
and configured port forwarding from Windows port 2222 to
CentOS port 22. The SSH connection then worked.

I checked GCC and Make, which were already installed.
I installed Git and checked that nano was available.
I created the project folder and my GitHub repository.

I calculated my personalised settings:
Registration number: IT24100639
Agent port: 9410
SID: 9360
Authentication token: OPS-0639

My next step is to implement the TCP connection between
the Agent and Controller.

## Five-client concurrency test — 3 October 2026

I tested five Controller processes connected to the Agent at the same time.
The ss command showed five established TCP connections on port 9410.
All five Controllers received OK AUTHENTICATED SID:9360.
After 180 seconds, each sent QUIT and received OK BYE SID:9360.

The Agent uses one detached pthread per client. Each client has its own
socket and authentication state, so waiting for one client's command
does not stop the Agent from handling other clients.
Detached threads release their thread resources when they finish.
Screenshots were saved separately for the report.

## SYSINFO implementation — 3 October 2026

Added SYSINFO using Linux sysinfo().
The response reports the one-minute load average, used RAM in MB,
and uptime in seconds. Used RAM is total RAM minus free RAM,
including memory used for caching. CPU load is not a percentage.

Testing: SYSINFO before authentication returned ERR 003 AUTH_REQUIRED.
After authentication, it returned OK SYSINFO 0.08 3443.56 12671 SID:9360.
QUIT returned OK BYE SID:9360.
Compilation completed without warnings using the selected GCC flags.

## LISTPROC implementation — 3 October 2026

Added an authenticated LISTPROC handler that reads process names
from /proc/<pid>/comm. It returns a snapshot of up to 20 readable
processes as comma-separated name/PID entries.

The response is bounded to fit one protocol line with the SID.
Special characters in names are replaced with underscores.
Processes that exit before their files can be read are skipped.

Testing: LISTPROC before authentication was rejected.
After authentication, it returned 20 process entries with SID:9360.
SYSINFO and QUIT still worked. Compilation produced no warnings.

## EXEC implementation — 4 October 2026

Added authenticated EXEC handlers for DATE, UPTIME, DISKFREE,
HOSTNAME and WHOAMI using C system functions.
Exact command names are required; extra arguments are rejected.
No shell is invoked.

DATE uses the Agent's local time. UPTIME reports seconds.
DISKFREE reports space available to the Agent user on the project
filesystem. HOSTNAME reports the system hostname. WHOAMI reports
the Agent's effective user using a reentrant user lookup.

Testing: EXEC before authentication was rejected.
All five allowed commands returned OK EXEC_RESULT with SID:9360.
EXEC LS, EXEC DATE extra and EXEC DATE;WHOAMI were rejected.
SYSINFO and QUIT continued to work.

## 2026-10-04 — TCP file transfers
Implemented authenticated PUT and GET with newline-delimited headers and exact-length binary payloads. The Controller calculates upload sizes and saves downloads in ./downloads. Agent files are stored in ./agentfiles/IT24100639. Transfers use 4096-byte chunks with a 100 MiB limit. Simple filenames prevent path traversal. Temporary files are renamed after complete transfers; interrupted transfers remove temporary files. Rejected PUT requests close the session to prevent payload bytes being interpreted as commands.

Tested unauthenticated GET, successful authentication, upload and download of a 44-byte text file, missing-file handling and QUIT. SHA-256 hashes matched for the original, Agent copy and downloaded copy.

## 2026-10-04 - UDP monitoring
Added a UDP monitoring worker for each TCP client session. The Agent sends SYSINFO datagrams every two seconds to the client's requested UDP port. The Controller displays these updates while accepting commands. Tested monitoring updates, MONITOR STOP, and a TCP SYSINFO command after stopping. The displayed responses included SID:9360.

## 2026-10-04 - Build automation
Added Makefile_639 to compile both programs with C11, compiler warnings, optimisation, and pthread support. Ran the clean and build targets successfully. Running make again reported that both programs were up to date.

## 2026-10-04 - Timestamped logging
Added mutex-protected log entries with local timestamps, SID, client address, event, and details. Authentication tokens are redacted. Tested failed and successful authentication, SYSINFO, a 28-byte upload and download, and QUIT. The log recorded PUT_COMPLETE, GET_COMPLETE, and DISCONNECT.
