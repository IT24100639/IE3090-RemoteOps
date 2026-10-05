# Reflection - IT24100639

## 1. AI tools and stages of use

I used ChatGPT/Codex during preparation, implementation, testing and
documentation. It helped explain the requirements, troubleshoot SSH,
and provided substantial C code drafts for the Agent and Controller.
It also assisted with the Makefile, logging, Python test scripts and
documentation. My work included updating project files in CentOS,
compiling and running the programs, performing tests, checking results,
collecting screenshots and maintaining Git commits. The prompt log
records the assistance and my verification.

## 2. What AI did well and where it caused confusion

AI was useful for breaking a large assignment into smaller steps.
Explanations of authentication, TCP file transfers and UDP monitoring
helped me connect the requirements to the program's behaviour.
Testing instructions helped me check both successful requests and
rejected requests instead of relying only on successful compilation.

However, the initial guidance suggested Mac and Kali, although my
environment was Windows and CentOS. Instructions were also sometimes
unclear about which terminal to use or where to paste code.
I needed more precise guidance. Receiving long code drafts helped
me progress, but made it difficult to understand every function.

## 3. What I changed, added or rejected

I rejected the suggested environment and continued with my existing
CentOS VM. During SSH troubleshooting, I changed VirtualBox networking
to NAT and configured port forwarding after checking the connection
problem. I requested clearer terminal instructions when needed.

I checked the implementation using observed responses, log entries
and file hashes. Validation included binary and empty files, invalid
paths, oversized requests, interrupted uploads, TCP stream boundaries
and independent monitoring sessions. I also requested changes to the
prompt log so it separated AI support from my practical work.
This made the record clearer without removing the assistance used.

## 4. Learning about my understanding

Understanding the C code was the hardest part for me. Managing
multiple terminals and testing file transfers were new experiences.
I now understand the Agent and Controller roles better, and why TCP
carries commands and files while UDP carries monitoring updates.
I learned that successful compilation alone does not prove correct
behaviour, and that responses, logs and hashes provide useful checks.

I still need more practice explaining the C implementation,
particularly socket operations and thread cleanup. Next time, I
would study the socket functions earlier, label my terminals clearly,
test one feature at a time and record results as I work.
