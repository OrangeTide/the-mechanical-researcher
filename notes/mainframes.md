# Mainframes

## What Is a Mainframe?

A mainframe is a large-scale computer designed for high-volume transaction
processing, bulk data handling, and mission-critical workloads. The word
"mainframe" originally referred to the large cabinet ("main frame") housing the
central processor, but now describes an entire class of machine.

Mainframes are not supercomputers. Supercomputers maximize floating-point
throughput for scientific computation. Mainframes maximize I/O throughput,
concurrent users, and uptime for business transactions. A single mainframe
routinely handles thousands of simultaneous users and billions of transactions
per day.

Mainframes are not ordinary servers scaled up. They differ in I/O architecture,
hardware redundancy, virtualization model, and backward compatibility guarantees
that have no equivalent in the x86 world.

## Why Mainframes Exist

Mainframes solve a specific set of problems:

- **Massive transaction throughput** -- processing billions of small
  transactions per day (credit card swipes, airline reservations, bank
  transfers) with consistent sub-second latency.
- **Near-zero downtime** -- organizations that cannot tolerate outages (banks,
  governments, airlines) need hardware-level fault tolerance.
- **Backward compatibility** -- code written in 1964 for the IBM System/360
  still runs on a 2025 IBM z17. This protects decades of software investment.
- **Regulatory compliance** -- pervasive encryption, auditing, and access
  controls built into the hardware and OS.

## RAS: Reliability, Availability, Serviceability

RAS is the core design philosophy of mainframe engineering.

- **Reliability** -- the probability the system produces correct results.
  Achieved through hardware self-checking, error-correcting codes (ECC)
  throughout the memory hierarchy, and redundant components.
- **Availability** -- the probability the system is operational at any given
  moment. Mainframes routinely achieve 99.999% uptime ("five nines"), meaning
  roughly five minutes of unplanned downtime per year. Maintained through
  seamless failover, hot-swappable components, and layered error recovery.
- **Serviceability** -- the ease and speed of diagnosing and repairing faults
  without disrupting service. Modular hardware, advanced diagnostics, and
  concurrent maintenance (replacing parts while running) make this possible.

## Vocabulary

| Term | Meaning |
|------|---------|
| **LPAR** | Logical Partition. A virtual division of a physical mainframe into independent systems, each running its own OS. Analogous to a VM but enforced in firmware. |
| **PR/SM** | Processor Resource/Systems Manager. The Type-1 (bare-metal) hypervisor that creates and manages LPARs. Introduced 1988. |
| **MIPS** | Million Instructions Per Second. On mainframes this is a *normalized* capacity metric, not a literal instruction count. Used for software licensing. |
| **MSU** | Million Service Units. Another normalized CPU capacity metric. Ratio is roughly 8 MIPS per 1 MSU. |
| **CPC** | Central Processor Complex. The physical processor unit(s) of a mainframe. |
| **Sysplex** | A cluster of interconnected z/OS systems that cooperate to run workloads. Up to 32 systems. |
| **Coupling Facility** | Dedicated hardware that coordinates data sharing and serialization across a Parallel Sysplex. |
| **Channel** | A dedicated I/O processor (implemented as a RISC core) that handles data transfer independently of the main CPU. This is what makes mainframe I/O fundamentally different from x86. |
| **DASD** | Direct Access Storage Device. The mainframe term for disk storage, connected via channels. Uses CKD (Count Key Data) track format. |
| **JCL** | Job Control Language. A scripting language for defining and submitting batch jobs. |
| **CICS** | Customer Information Control System. The most widely used online transaction processing (OLTP) server on mainframes. |
| **IMS** | Information Management System. An older hierarchical database with a built-in transaction manager (IMS/TM). Still in heavy use. |
| **DB2** | IBM's relational (SQL) database for z/OS. Increasingly replacing IMS for workloads that need ad-hoc queries. |
| **TSO** | Time Sharing Option. Interactive command-line access to z/OS. |
| **ISPF** | Interactive System Productivity Facility. A menu-driven text UI layered on top of TSO, used for editing files, submitting jobs, and browsing output. |
| **z/OS** | The flagship 64-bit mainframe operating system. Descendant of OS/360 (1966) -> MVS -> OS/390 -> z/OS. |
| **z/VM** | A virtualization operating system that can host multiple guest OSes (z/OS, Linux, z/VSE, z/TPF) simultaneously. |
| **z/VSE** | A smaller mainframe OS descended from DOS/360. Less common today. |
| **z/TPF** | Transaction Processing Facility. A purpose-built OS for ultra-high-volume OLTP, used by airlines and financial networks. |
| **USS** | UNIX System Services. A POSIX-compliant UNIX environment within z/OS. Provides shells, file systems, and standard UNIX APIs. |
| **WLM** | Workload Manager. A z/OS component that dynamically allocates CPU, memory, and I/O to meet service-level goals. |
| **Batch processing** | Running a series of jobs (programs) without interactive user involvement. Read input, process, write output. Traditionally run overnight or in scheduled windows. |
| **OLTP** | Online Transaction Processing. Real-time, interactive transaction handling -- the opposite of batch. |

## Architecture

### Processors

Modern IBM mainframes use custom-designed processors:

- **z17** (April 2025): Telum II processor at 5.5 GHz, up to 208 cores, 64 TB
  max memory. Includes on-chip AI accelerators capable of 450 billion inference
  operations per day at 1 ms latency.
- **z16** (2022): Telum processor at 5.2 GHz, up to 200 cores (multi-frame),
  40 TB max memory. 300 billion inference operations per day.

The instruction set is z/Architecture -- a 64-bit CISC design descended from
System/360. The "z" stands for "zero downtime."

### Channel I/O

The most distinctive architectural feature. Instead of sharing a PCIe bus with
the CPU (as x86 does), mainframes use dedicated I/O processors called
*channels*. Each channel is an independent RISC core that handles data movement
between memory and peripherals without consuming main CPU cycles. This is why
mainframes sustain microsecond-level I/O latency even under extreme load.

### Virtualization

Mainframes pioneered virtualization in the 1960s (CP-67/CMS). The current
model:

1. **PR/SM** -- firmware-level hypervisor partitions the hardware into LPARs.
   Each LPAR is fully isolated with its own virtual processors, memory, and I/O
   paths. Modern systems support 60-85 LPARs.
2. **z/VM** -- a software hypervisor that runs *inside* an LPAR and can host
   hundreds or thousands of guest virtual machines. This is how Linux instances
   typically run on mainframes.

The two levels can be combined: PR/SM creates LPARs, one LPAR runs z/VM, and
z/VM hosts many Linux guests.

### Operating Systems

| OS | Lineage | Purpose |
|----|---------|---------|
| **z/OS** | OS/360 -> MVS -> OS/390 -> z/OS | General-purpose production: batch, OLTP, databases |
| **z/VM** | CP-67 -> VM/370 -> z/VM | Virtualization host for running multiple guest OSes |
| **z/VSE** | DOS/360 -> z/VSE | Smaller workloads, less common today |
| **z/TPF** | ACP -> TPF -> z/TPF | Ultra-high-volume transaction processing (airlines, payment networks) |
| **Linux on Z** | Standard Linux kernel | RHEL, SUSE, Ubuntu certified for s390x architecture |

### Data Management

- **IMS** (1968): Hierarchical database. Pre-defined access paths make common
  operations very fast. Still handles massive transaction volumes in banking.
- **DB2**: Relational/SQL database. Supports ad-hoc queries. Many organizations
  are migrating from IMS to DB2.
- **CICS**: Not a database but a transaction server -- it manages the lifecycle
  of OLTP interactions and connects to IMS or DB2 on the backend.

## History

### The System/360 (1964)

The most important product in IBM's history. Announced April 7, 1964, with
deliveries starting in 1965.

- **Investment**: $5 billion over four years -- internally called "betting the
  company." Adjusted for inflation, comparable to the Apollo program.
- **Key people**: Gene Amdahl (chief hardware architect, also designed the IBM
  704), Fred Brooks (project manager, later wrote *The Mythical Man-Month*).
- **Innovation**: First computer family that separated software from hardware.
  Programs written for any System/360 model ran on every other model. This was
  revolutionary -- before S/360, each new machine required rewriting all
  software.
- **Impact**: By 1989, S/360-derived products accounted for over half of IBM's
  total revenue.

### Architecture Evolution

| Year | Architecture | Key Advance |
|------|-------------|-------------|
| 1964 | System/360 | 8-bit byte addressing, family compatibility |
| 1972 | System/370 | Virtual memory (Dynamic Address Translation) |
| 1981 | 370-XA | Extended addressing: 24-bit to 31-bit |
| 1988 | ESA/370 | Multiple address spaces, access registers |
| 1990 | ESA/390 | Fiber-optic ESCON channels |
| 2000 | z/Architecture | 64-bit addressing (z900). Full backward compatibility with all predecessors. |
| 2022 | z16 | On-chip AI inference, quantum-safe cryptography |
| 2025 | z17 | Telum II, 208 cores, 64 TB memory, enhanced AI |

Every generation maintained backward compatibility with System/360. A COBOL
program compiled in 1965 can run on a z17 without modification.

### Industry Leaders

**IBM** has dominated mainframes from the beginning and is effectively the only
remaining vendor of z/Architecture systems. Current market share is
overwhelming.

**Historical competitors**:

- **Amdahl Corporation** (1970-2000s) -- Founded by Gene Amdahl after leaving
  IBM. Built IBM-compatible ("plug-compatible") mainframes that were faster and
  cheaper. Gained double-digit market share in the early 1980s. Became a
  Fujitsu subsidiary in 1997. Abandoned 64-bit mainframe development when IBM
  launched z/Architecture in 2000 -- estimated cost to build a compatible
  64-bit system exceeded $1 billion.

- **Hitachi, Fujitsu, NEC** -- Japanese manufacturers that built
  plug-compatible mainframes, some under technology-sharing agreements. Their
  US presence was through partners like National Advanced Systems (NAS).

- **The BUNCH** -- Burroughs, UNIVAC, NCR, Control Data, Honeywell. IBM's main
  competitors in the 1960s-1980s. All had their own incompatible architectures.

- **Unisys** (1986-present) -- Formed from the merger of Burroughs and Sperry
  (which had absorbed UNIVAC). Still sells ClearPath mainframes with their own
  operating systems (MCP from Burroughs, OS 2200 from Sperry), but these are a
  niche market.

- **Bull** (France) -- Sold GCOS mainframes. Now part of Atos.

## Who Uses Mainframes Today

| Sector | Usage |
|--------|-------|
| Banking/Finance | ~33% of the mainframe market. Handles 90% of global credit card transactions, 70% of financial transaction value. |
| Insurance | Policy management, claims processing, actuarial computations. |
| Airlines | 4 of the top 5 global airlines run on mainframes. Reservations, ticketing, scheduling. |
| Government | Tax processing, benefits administration, census, defense. |
| Retail | Payment processing, inventory management. |

### Why They Have Not Been Replaced

- **Cost at scale**: A single z17 with 208 cores can replace hundreds of x86
  servers. For sustained high-volume transaction workloads, mainframes have
  lower total cost of ownership. Multiple organizations have attempted full
  cloud migrations and reversed course when costs ballooned.
- **Decades of software**: Organizations have millions of lines of COBOL, PL/I,
  and assembler code that works correctly and is deeply integrated with
  business processes. Rewriting is risky and expensive.
- **Reliability**: Five-nines availability is standard, not aspirational. Cloud
  providers offer 99.99% SLAs (four nines) with caveats.
- **Regulatory requirements**: Built-in pervasive encryption, hardware security
  modules, and audit trails satisfy compliance requirements that would be
  complex to replicate on distributed systems.

## Modern Relevance

Mainframes are not static. Recent developments:

- **AI on Z**: The z16 and z17 have on-chip AI accelerators for real-time
  inference (fraud detection during transaction processing, not after).
- **Quantum-safe cryptography**: The Crypto Express 8S module implements
  NIST-standardized post-quantum algorithms (CRYSTALS-Kyber, CRYSTALS-Dilithium)
  to protect against "harvest now, decrypt later" attacks.
- **Linux on Z**: Full Linux distributions (RHEL, SUSE, Ubuntu) run natively on
  s390x. Hundreds of open-source packages are available.
- **Containers**: zCX (z/OS Container Extensions) runs Docker-compatible Linux
  containers directly on z/OS without a separate Linux LPAR.
- **Hybrid cloud**: IBM z/OS Cloud Broker integrates mainframe resources with
  Red Hat OpenShift, letting developers access z/OS services without deep
  mainframe expertise.
- **Open-source tooling**: Zowe provides an open-source DevOps framework for
  mainframe development. Python and Java are available on z/OS.

## Mainframes vs. x86/Cloud

| Aspect | Mainframe | x86/Cloud |
|--------|-----------|-----------|
| **Scaling** | Vertical (bigger machine) | Horizontal (more machines) |
| **I/O model** | Dedicated channel processors | Shared PCIe bus |
| **Availability** | 99.999% standard | 99.99% typical |
| **Latency under load** | Microsecond-level, stable | Degrades with contention |
| **Instruction set** | z/Architecture (CISC, 64-bit) | x86-64 |
| **Backward compat** | 60+ years (S/360 to z17) | Periodic breaks |
| **Entry cost** | Millions of dollars | Low per-unit |
| **Virtualization** | PR/SM firmware + z/VM, since 1960s | Software hypervisors (KVM, VMware, Hyper-V) |
| **Best fit** | Sustained high-volume transactions | Variable/elastic workloads, web-scale |

## Common Misconceptions

**"Mainframes are dead."** The global mainframe market was $3.3-3.7 billion in
2024. IBM launched the z17 in April 2025. Mainframes process 30+ billion
transactions daily, including 90% of credit card transactions worldwide.

**"Cloud is always cheaper."** At high sustained utilization, mainframes have
lower TCO for transaction workloads. Cloud excels at elastic and variable
workloads. Some organizations have reversed cloud migrations after costs
exceeded projections, though full reversals remain exceptional.

**"Nobody writes new mainframe code."** Java, Python, and Node.js all run on
z/OS. Containerized Linux workloads on z/VM are common. The platform is not
frozen in 1975.
