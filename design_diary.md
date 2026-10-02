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
