# AI Prompt Log

## 2 October 2026 - Assignment preparation

Tool: ChatGPT

Prompt:
"what is this assignment? what i need to do? how long it will take to finish it?"

Output used:
ChatGPT explained the required features, deliverables,
deadline, lab assessment and viva.

Checks:
I used the assignment brief to identify the required work.

## 2 October 2026 - Step-by-step guidance

Tool: ChatGPT

Prompt:
"step by step very clearly tell me how to do it.(how to connect ssh connection, how to do assignment... everything)"

Output used:
I followed instructions to check my existing CentOS tools
and prepare my project folder and Git repository.

Changes:
ChatGPT initially suggested a Mac and Kali setup.
I clarified that I use Windows and CentOS 10.

## 2 October 2026 - SSH troubleshooting

Tool: ChatGPT

Prompt:
"i couldnt connect ssh connection"

Output used:
ChatGPT suggested checking the SSH service and network.
After reviewing my screenshots, it suggested NAT with
port forwarding from host port 2222 to guest port 22.

Checks:
I ran the suggested commands and confirmed that SSH worked.

## 2 October 2026 - Documentation assistance

Tool: ChatGPT

Prompt:
"i typed this code. after that what i need to do?"

Output used:
ChatGPT drafted an initial design diary entry and explained
how to paste, save and check the file using nano and cat.

Checks or changes:
I checked the entry against the steps I completed.

## Concurrency test guidance — 3 October 2026

Prompt summary: Continue the assignment with clear step-by-step commands.

AI assistance: Provided commands to launch five existing C Controllers
in the background, authenticate each, keep connections open for 180
seconds, inspect connections using ss, and read their saved output.
Also helped interpret the screenshots and draft this diary entry.

Validation: I ran the commands in CentOS. Five established connections,
five successful authentication responses, and five BYE responses were
observed.

## SYSINFO code assistance — 3 October 2026

User prompt: "can u give me the code fully , then i can copy paste directly and fully"

AI assistance: Supplied the complete updated agent_639.c with a
SYSINFO handler using Linux sysinfo(), retaining authentication
and threaded client handling. Explained the returned values
and provided compilation and testing instructions.

Validation: I compiled and ran the supplied code in CentOS.
Unauthenticated SYSINFO was rejected. Authenticated SYSINFO
returned system statistics with SID:9360. QUIT succeeded.

## LISTPROC code assistance — 3 October 2026

Prompt context: Continue the assignment using complete code
that can be copied and pasted.

AI assistance: Supplied the complete updated Agent code with
a LISTPROC handler reading /proc, a 20-process snapshot limit,
bounded response construction, and process-name sanitisation.
Provided compilation and authentication test instructions.

Validation: I compiled and ran the code in CentOS.
LISTPROC was rejected before authentication and returned process
names/PIDs after authentication. SYSINFO and QUIT also succeeded.

## EXEC code assistance — 4 October 2026

User prompt: "tell me very clearly. give me the whole code to copy paste"

AI assistance: Supplied complete Agent code containing the existing
features and five allowlisted EXEC handlers implemented through C
system functions. Provided instructions for compilation, running
the Agent and Controller in separate SSH tabs, and testing.

Validation: I ran the code in CentOS. All five allowed commands
succeeded after authentication. Unauthenticated EXEC, unknown
commands, extra arguments and a semicolon command were rejected.
SYSINFO and QUIT also succeeded.

## 2026-10-04 — File transfer implementation
Prompt: "dont tell like this , just give me the whole code every time. then i can copy paste it . and tell me where i need to paste it"
Context: Requested complete copy-paste code and clear terminal instructions while adding PUT and GET.
AI assistance: Provided an exact-byte receive helper, file transfer handlers, complete Agent and Controller replacements, and testing instructions.
Validation: Compiled without reported errors and tested authentication protection, upload, download, missing-file handling and QUIT. Verified matching SHA-256 hashes for all three file copies.

## 2026-10-04 - UDP monitoring assistance
Requested complete copy-and-paste code and step-by-step instructions for adding UDP monitoring to the Agent and Controller. AI assisted with the monitoring worker, UDP reception, and test instructions. I compiled the programs and checked the displayed monitoring updates, stop response, and subsequent TCP SYSINFO response.

## 2026-10-04 - Makefile assistance
AI provided the complete Makefile_639 and commands to build both programs. I created the file and checked the clean, build, and up-to-date results in CentOS.

## 2026-10-04 - Logging assistance
AI provided the complete logging helper, updated Agent, file-transfer header, and Makefile. I compiled both programs and tested authentication, SYSINFO, upload, download, and disconnection. I checked the resulting timestamped log entries.

## 2026-10-04 - File transfer robustness testing
Tool: ChatGPT / Codex.

Requests included:
- "why do we need python?"
- Guidance on testing oversized and interrupted uploads.
- Checking screenshots of the test results and Git status.

AI contribution:
Explained why direct socket tests were needed to check the Agent
independently of Controller-side validation. Generated the complete
test_upload_errors.py script and commands to run it. Helped interpret
the output and prepare this testing record.

Validation:
Ran the script on CentOS using Python 3.12.13 against the C Agent.
The oversized-upload, interrupted-upload cleanup and subsequent
SYSINFO checks all passed. Screenshot 17 records the output.

## 2026-10-04 - TCP stream test script
Tool: ChatGPT / Codex.
AI generated the complete test_tcp_stream.py script and execution
instructions as part of the continuing assignment guidance.
The script tests split commands, multiple commands in one send,
and binary file payload boundaries with subsequent commands.
I ran it against the C Agent on CentOS. All three checks passed,
and the output was captured in screenshot 18.

## 2026-10-04 - Monitoring session tests
Tool: ChatGPT / Codex.
AI generated the complete test_monitor_sessions.py script and
instructions to test five connected clients, two independent UDP
monitoring sessions, MONITOR STOP, QUIT and TCP disconnect cleanup.
I ran the script against the C Agent on CentOS. All checks passed.
Screenshot 19 records the results.
