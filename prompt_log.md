# AI Prompt Log - IT24100639

## Scope of assistance

I used ChatGPT/Codex for assignment guidance, troubleshooting,
substantial code drafts, test scripts and documentation drafts.
I carried out the project steps in my CentOS environment, including
updating files, compiling and running programs, executing tests,
checking results, collecting evidence and maintaining Git commits.

The entries below separate the AI support from my work and verification.
The prompts below are summarised rather than quoted verbatim.

## 2 October 2026 - Assignment preparation

Tool: ChatGPT

Prompt summary:
Requested an explanation of the assignment requirements and estimated completion time.

Task:
Identify the assignment requirements and deliverables.

AI support:
Explained the required features, submission materials, deadline,
lab assessment and viva.

My work and verification:
Consulted the assignment brief and identified the work to complete.

## 2 October 2026 - Environment and Git preparation

Tool: ChatGPT

Prompt summary:
Requested step-by-step guidance for SSH access and project preparation.

Task:
Prepare the project using my existing Windows and CentOS environment.

AI support:
Provided commands for checking tools, creating the project directory
and configuring Git. Initially suggested a different environment.

My work and verification:
Clarified that I use Windows and CentOS 10, checked the installed
tools, prepared the project files and configured my Git repository.

## 2 October 2026 - SSH troubleshooting

Tool: ChatGPT

Prompt summary:
Requested troubleshooting assistance for a failed SSH connection.

Task:
Restore SSH access to the CentOS VM.

AI support:
Suggested checking the SSH service and network configuration,
then using NAT with host port 2222 forwarded to guest port 22.

My work and verification:
Checked the VM configuration, applied the network changes and
confirmed that Windows Terminal could connect through SSH.

## 2 October 2026 - Initial documentation

Tool: ChatGPT

Prompt summary:
Requested guidance on saving and checking the initial design diary.

Task:
Record the environment preparation in the design diary.

AI support:
Drafted an initial diary entry and explained how to save and
inspect it using nano and cat.

My work and verification:
Saved the diary and checked its description against the steps
I had completed.

## 2-3 October 2026 - TCP, authentication and concurrency

Tool: ChatGPT / Codex

Prompt summary:
Requested clear step-by-step instructions and complete code
that could be copied into the project files.

Task:
Establish communication between the C Agent and Controller,
add authentication and support concurrent clients.

AI support:
Provided Agent, Controller and socket-helper code drafts,
including threaded client handling and authentication.
Provided commands to run five C Controllers simultaneously
and inspect their connections and saved responses.

My work and verification:
Created and updated the files in CentOS, compiled and ran the
programs, and tested authentication responses.
Ran five Controllers, observed five established connections,
and checked five successful authentication and BYE responses.

## 3 October 2026 - SYSINFO

Tool: ChatGPT / Codex

Prompt summary:
Requested an updated code draft and guidance for implementing and testing SYSINFO.

Task:
Add system statistics to the Agent.

AI support:
Provided an updated Agent code draft using Linux sysinfo(),
with explanations of the values and test instructions.

My work and verification:
Updated the project file, compiled and ran the programs,
and tested SYSINFO before and after authentication.
Checked that authenticated responses included statistics
and SID:9360, and that QUIT still worked.

## 3 October 2026 - LISTPROC

Tool: ChatGPT / Codex

Prompt summary:
Requested complete updated code and instructions for the next feature.

Task:
Return a snapshot of process names and PIDs.

AI support:
Provided an updated Agent code draft reading /proc, with a
20-process limit, bounded output and process-name sanitisation.

My work and verification:
Updated and compiled the Agent, tested LISTPROC before and
after authentication, and checked the returned process entries.
Also checked SYSINFO and QUIT.

## 4 October 2026 - EXEC commands

Tool: ChatGPT / Codex

Prompt summary:
Requested an updated code draft and instructions for implementing and testing EXEC commands.

Task:
Support the five allowed EXEC commands.

AI support:
Provided an updated Agent code draft implementing DATE, UPTIME,
DISKFREE, HOSTNAME and WHOAMI through C APIs, plus test commands.

My work and verification:
Updated, compiled and ran the programs in separate SSH terminals.
Tested all five allowed commands and checked their responses.
Tested unauthenticated requests, unknown commands, extra arguments
and a semicolon command, and checked that they were rejected.

## 4 October 2026 - PUT and GET

Tool: ChatGPT / Codex

Prompt summary:
Requested updated Agent and Controller code drafts and instructions for adding PUT and GET.

Task:
Add binary file uploads and downloads.

AI support:
Provided updated Agent and Controller code drafts, file-transfer
handlers and an exact-byte receive helper, with testing instructions.

My work and verification:
Updated the project files and compiled the programs.
Tested authentication protection, upload, download, missing files
and QUIT. Compared SHA-256 hashes of the original, stored and
downloaded copies and confirmed that they matched.

## 4 October 2026 - UDP monitoring

Tool: ChatGPT / Codex

Prompt summary:
Requested complete code and step-by-step monitoring instructions.

Task:
Receive periodic system statistics through UDP.

AI support:
Provided monitoring-worker and Controller reception code drafts,
along with commands for checking monitoring behaviour.

My work and verification:
Updated and compiled the programs, started monitoring and
checked the displayed UDP statistics and SID.
Stopped monitoring and checked that TCP SYSINFO still worked.

## 4 October 2026 - Build automation

Tool: ChatGPT / Codex

Prompt summary:
Requested guidance for adding the personalised Makefile.

Task:
Build both C programs consistently.

AI support:
Provided a Makefile_639 draft and build commands.

My work and verification:
Created the Makefile and ran clean and build targets.
Checked that both programs compiled and that another make
invocation reported them as up to date.

## 4 October 2026 - Timestamped logging

Tool: ChatGPT / Codex

Prompt summary:
Requested code and instructions for adding logging.

Task:
Record connection, command, transfer and disconnection events.

AI support:
Provided logging-helper code and updates to the Agent,
file-transfer header and Makefile.

My work and verification:
Updated and compiled the project. Tested failed and successful
authentication, SYSINFO, upload, download and QUIT.
Inspected the log for timestamps, SID, transfer completion,
disconnection events and authentication-token redaction.

## 4 October 2026 - File-transfer validation

Tool: ChatGPT / Codex

Prompt summary:
Requested an explanation of Python socket tests for validating the C Agent.

Other requests, summarised:
Asked for oversized and interrupted-upload testing guidance
and interpretation of test results.

Task:
Check binary transfers, invalid requests and upload cleanup.

AI support:
Explained direct socket testing and generated
test_upload_errors.py with execution instructions.

My work and verification:
Tested a 65,536-byte binary file and a zero-byte file.
Compared file hashes and checked invalid-path and missing-file
responses. Ran the Python script against the C Agent.
Oversized-upload rejection, interrupted-upload cleanup and
subsequent SYSINFO checks passed.
Screenshots 14-17 record the evidence.

## 4 October 2026 - TCP stream framing

Tool: ChatGPT / Codex

Prompt summary:
Requested further checks of TCP command and payload handling.

Task:
Check split commands, combined commands and file boundaries.

AI support:
Generated test_tcp_stream.py and instructions for running it.

My work and verification:
Ran the script against the C Agent in CentOS.
All three checks passed, covering split command text,
multiple commands sent together and binary payload boundaries.
Screenshot 18 records the results.

## 4 October 2026 - Monitoring session isolation

Tool: ChatGPT / Codex

Prompt summary:
Requested testing of concurrent monitoring sessions and cleanup.

Task:
Check that stopping one monitoring session does not affect another.

AI support:
Generated test_monitor_sessions.py and execution instructions.

My work and verification:
Ran the script with five authenticated TCP clients and two
UDP monitoring sessions. Checked session independence and
cleanup after MONITOR STOP, QUIT and TCP disconnection.
All checks passed. Screenshot 19 records the results.

## 5 October 2026 - Documentation and evidence review

Tool: ChatGPT / Codex

Requests included:
- Reviewing README, design diary and prompt log text.
- Guidance on capturing code and execution screenshots.
- Revising the prompt log to describe both AI support and my work.

Task:
Prepare accurate documentation and submission evidence.

AI support:
Drafted README and diary updates from the development records
and test results, reviewed pasted documentation, and identified
source sections to capture. Drafted this revised prompt log.

My work and verification:
Saved and inspected the documents, retained the longer diary
in docs/development_history.md, captured code screenshots,
and ran the final clean build without warnings or errors.
Captured Agent startup and storage/log evidence.
Report preparation, reflection and final packaging are still pending.
