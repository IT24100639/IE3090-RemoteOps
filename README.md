# RemoteOps - IE3090 Assignment

Registration number: IT24100639

Repository: https://github.com/IT24100639/IE3090-RemoteOps

## Overview

RemoteOps consists of an Agent and Controller written in C using
BSD sockets on Linux. TCP carries commands and binary file transfers.
UDP carries periodic system statistics.

The Agent creates a detached client thread for each TCP connection.
Authentication and monitoring state belong to each client session.
A separate monitoring thread sends UDP updates every two seconds.
A mutex protects log writes from concurrent threads.

## Personalised settings

| Setting | Value |
|---|---|
| Agent TCP port | 9410 (7000 + 2410) |
| SID tag | SID:9360 |
| Authentication token | OPS-0639 |
| Agent source | agent_639.c |
| Controller source | controller_639.c |
| Makefile | Makefile_639 |
| Runtime log | remoteops_IT24100639.log |
| Agent storage | ./agentfiles/IT24100639/ |
| Controller downloads | ./downloads/ |
| Maximum file size | 100 MiB (104857600 bytes) |
| Monitoring interval | 2 seconds |
| Submission archive | IE3090_IT24100639.zip |

## Requirements

- Linux; developed and tested on CentOS 10.
- GCC, GNU Make and POSIX threads.
- Python 3 only for the optional test scripts.

Both the Agent and Controller run inside the CentOS VM.
Windows Terminal provides SSH access to that VM.

## Build

Run these commands from the project directory:

```bash
mkdir -p agentfiles/IT24100639
make -f Makefile_639
```

To remove compiled programs:

```bash
make -f Makefile_639 clean
```

## Run

Terminal 1, from the project directory:

```bash
./agent_639
```

Terminal 2, from the same project directory:

```bash
./controller_639 127.0.0.1
```

The Controller also accepts an accessible Agent IPv4 address:

```bash
./controller_639 <agent_ipv4>
```

The Agent listens on TCP port 9410. Connections between separate
machines require network access to that port and to the selected
Controller UDP listening port.

## Controller commands

Enter commands at the remoteops> prompt, one at a time:

```text
AUTH OPS-0639
SYSINFO
LISTPROC
EXEC DATE
EXEC UPTIME
EXEC DISKFREE
EXEC HOSTNAME
EXEC WHOAMI
MONITOR START 9639
MONITOR STOP
QUIT
```

Authenticate before using other commands.

SYSINFO returns one-minute CPU load average, used memory in MiB
and system uptime in seconds. CPU load is not CPU usage percentage.
Used memory is calculated as total RAM minus free RAM.

LISTPROC returns a snapshot of up to 20 process names and PIDs.

EXEC accepts only the five exact command names shown above.
The Agent obtains their results through C system APIs.
Unknown commands, additional arguments and shell command strings
are rejected.

MONITOR START uses the supplied UDP port on the TCP client's IP
address. Each monitoring Controller must use a different available
UDP listening port. Updates stop after MONITOR STOP, QUIT, failed
reauthentication or TCP disconnection.

## File transfers

Place a local file in the Controller's project directory.
For example, create a sample from a normal shell prompt:

```bash
printf 'RemoteOps file transfer test\n' > sample.txt
```

At the authenticated Controller prompt:

```text
PUT sample.txt
GET sample.txt
```

The Controller determines the upload size automatically.
The Agent stores the upload in ./agentfiles/IT24100639/.
The Controller saves downloads in ./downloads/.

Filenames must contain only ASCII letters, digits, underscores,
hyphens and dots, must not start with a dot, and must be at most
128 characters long. Paths and filenames containing spaces are
not supported.

Uploads use temporary files and an atomic rename after completion.
Interrupted uploads remove their temporary files.
A completed PUT can replace an existing file with the same name.

## Wire protocol

TCP command lines end with a newline. Every TCP response line ends
with SID:9360 and a newline.

File transfers use a line header followed by the exact number
of raw payload bytes:

```text
Controller -> Agent: PUT <filename> <size>\n<raw bytes>
Agent -> Controller: OK FILE_RECEIVED <filename> SID:9360\n

Controller -> Agent: GET <filename>\n
Agent -> Controller: OK FILE_SEND <filename> <size> SID:9360\n<raw bytes>
```

UDP monitoring datagrams use:

```text
SYSINFO <cpu_load> <memory_MiB> <uptime_seconds> SID:9360\n
```

The send helper loops until all bytes have been sent.
The line receiver reads through the newline without consuming
the following file payload. The exact-byte receiver handles
partial reads during transfers.

## Error handling

Examples include:

- ERR 001 AUTH_FAILED
- ERR 002 COMMAND_NOT_ALLOWED
- ERR 003 AUTH_REQUIRED
- ERR 004 FILE_TOO_LARGE
- ERR 005 FILE_NOT_FOUND
- ERR 006 LINE_TOO_LONG
- ERR 010 INVALID_FILE_REQUEST

Responses include the SID tag and newline.
Rejected PUT requests close the connection to prevent unread upload
data from being interpreted as another command. Missing or invalid
GET requests return an error while allowing further commands.

## Logging

The Agent appends timestamped connection, command, response,
file-transfer and disconnection events to:

```text
remoteops_IT24100639.log
```

Entries contain the SID and peer address.
AUTH tokens are redacted in command log entries.
The runtime log, compiled programs and generated transfer data
are excluded from Git.

## Tests

Start the Agent in another terminal, then run these scripts from
the project directory:

```bash
python3 test_upload_errors.py
python3 test_tcp_stream.py
python3 test_monitor_sessions.py
```

The scripts check:

- Oversized upload rejection and interrupted-upload cleanup.
- Agent operation after rejected and interrupted uploads.
- Commands split across sends and multiple commands sent together.
- Binary payload boundaries and subsequent TCP commands.
- Five simultaneous authenticated TCP client sessions.
- Independent UDP monitoring sessions.
- Monitoring cleanup after STOP, QUIT and TCP disconnection.

Manual tests also covered authentication, SYSINFO, LISTPROC,
all five allowed EXEC commands, rejected EXEC requests, ordinary
file transfers, matching binary SHA-256 hashes, zero-byte files,
invalid paths and missing files.

Evidence screenshots are included in the report preparation
workflow; they are not currently stored in this repository.

## Assumptions and limitations

- Run the programs from the project directory.
- Create the Agent storage directory before starting the Agent.
- TCP commands are case-sensitive.
- UDP updates are best effort and can be lost.
- Transfers use a shared registration-specific storage directory.
- Concurrent completed uploads to the same filename may replace
  one another; the last successful rename determines its content.
- No application-level idle timeout or maximum client-thread
  count is implemented.
- The token and traffic are unencrypted; this is a coursework
  implementation intended for the lab environment.
- The tests establish the observed behaviours, not exhaustive
  coverage of every possible input or network failure.

## AI assistance

ChatGPT/Codex provided substantial assistance with implementation,
test scripts, documentation and troubleshooting.
See prompt_log.md for the recorded assistance and validation,
and design_diary.md for development decisions and progress.
