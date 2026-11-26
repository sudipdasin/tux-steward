# SysPulse alpha.3: manual updates and tool availability

## Check for a newer application release

Open **Settings & help → Updates → Check for updates**. Nothing contacts a release service when SysPulse starts or when this page opens. An explicit check reads up to 20 recent public releases from this project's GitHub API. It sends a fixed user-agent string and no diagnostics or authentication token. The request has a ten-second absolute deadline, a 512 KiB body limit, TLS verification and no automatic redirects. Cancel check stops the request and clears any earlier offer.

Version comparison uses numeric major/minor/patch and alpha/beta/rc sequence ordering. Stable installations do not offer testing builds by default. Only published, consistently labeled versions with the exact HTTPS project release URL are eligible. Invalid responses and connection/rate-limit errors are shown as failures, not as proof that the application is current. No newer release found means only that no eligible newer version appeared in the bounded recent list.

A newer release enables **Review release notes**. This opens the validated project page in the normal browser when clicked. Read the target distribution, checksums and known limits before downloading a package. This milestone does not download, authenticate, install or roll back an update. Signed package/update distribution and application-owned installation remain separate work. Existing system authorization and package-manager review still apply.

## Audit local tool availability

Open **Settings & help → Tool availability → Refresh availability**. The bounded local audit checks the existing dependencies/session requirements for all 35 catalog tools, including Experimental tools. It never executes a tool, requests elevation or installs a dependency. Search matches tool names, groups and missing-dependency explanations. The summary shows dependency counts and the snapshot time; refresh explicitly after installing a component or changing sessions.

**Dependencies found** is a file/session prerequisite result. It does not prove permissions, a functioning PolicyKit agent, active systemd services, device support or successful operation. **Unavailable** includes the concrete detected reason. **Inspect selected tool** opens the corresponding existing workflow without executing it. Experimental changes retain their in-app review and capability limits.

## Native XFCE settings launchers

Display, keyboard, mouse, power, panel, session, appearance and window-manager buttons resolve only the fixed allowed programs from protected system locations. They recheck the trusted executable when clicked. A similarly named file in a user-writable PATH directory cannot replace a native settings launcher. Missing/untrusted tools remain unavailable; SysPulse does not install them silently.

The approved Precision interface, resource monitoring, process identity guards, startup overrides, service-action admission, 35 catalog tools, reports and three legacy compatibility applications are retained. There is no automatic maintenance, telemetry or background self-update. This remains experimental alpha software; actual Mint hardware and full authorization/login integration require separate verification.
