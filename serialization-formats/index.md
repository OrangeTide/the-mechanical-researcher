---
title: "Serialization Formats: From Xerox Courier to Cap'n Proto"
date: 2026-04-02
abstract: A comparative survey of serialization and messaging formats across five decades — from Sun's XDR and ASN.1 to modern zero-copy systems — examining wire formats, type systems, and the trade-offs that shaped distributed computing.
category: systems
---

## Introduction

Every distributed system must answer the same question: how do you turn structured data into bytes, send those bytes over a wire, and reconstruct the data on the other side? The answer — serialization — is one of the oldest problems in computing, and the solutions tell a story about the priorities of each era.

The 1980s cared about heterogeneous hardware: VAX, SPARC, 68k, and Cray machines all had different byte orders, alignment rules, and floating-point formats. The 1990s cared about objects and middleware. The 2000s cared about web services and human readability. Today, the priorities are speed, compactness, and supporting hundreds of microservices across dozens of programming languages.

This article surveys over 35 serialization formats and RPC systems, from S-expressions (1958) and CSV (1972) through the zero-copy and columnar formats of the 2010s. The focus is on binary formats practical for internet-connected devices and cloud infrastructure, but we include historical, text-based, and analytics-oriented formats where they illuminate the design lineage.

## The RPC Lineage

The history of serialization is inseparable from the history of remote procedure calls. Almost every major serialization format was created to serve an RPC system, and the design constraints of the RPC shaped the wire format.

### Xerox Courier (1981) — The First RPC Protocol

The story begins at Xerox PARC. Courier was part of the Xerox Network Systems (XNS) protocol suite, designed to map directly to Mesa programming language function calls. Mesa's module system — where every library has a "definitions file" specifying the interface plus one or more program files for the implementation — was the conceptual model for Courier's interface descriptions.

Courier identified remote procedures by a **(program number, version number, procedure number)** triple and ran atop XNS's Sequenced Packet Protocol. It had a bulk data transfer mechanism for moving large payloads alongside function calls. But Courier had no automatic stub compiler — applications serialized and deserialized function calls manually.

The missing piece arrived in 1984. Andrew Birrell and Bruce Nelson, also at PARC, published "Implementing Remote Procedure Calls" in ACM TOCS, describing the Cedar RPC mechanism with automatic stub generation via a compiler called "Lupine." This paper formalized the RPC paradigm: define an interface, generate client stubs that marshal arguments, generate server skeletons that dispatch calls. Every RPC system since follows this blueprint.

### Sun ONC RPC and XDR (1984–1988)

Sun Microsystems built ONC RPC explicitly modeled on Xerox Courier. The system had three components:

**XDR (External Data Representation)** defined the wire format. Formalized in RFC 1014 (June 1987) and updated as RFC 4506 (May 2006), XDR made a simple, consequential choice: **big-endian, 4-byte aligned, sender-makes-right**. Every sender converts to the canonical format; every receiver converts from it. The 4-byte alignment was a deliberate compromise — "big enough to support most architectures efficiently, except for rare machines such as the eight-byte-aligned Cray, and small enough to keep the encoded data restricted to a reasonable size."

XDR is implicitly typed — no type tags appear on the wire. Both sides must agree on the data layout in advance. The rationale: "data-typing has a relatively high cost for what small advantages it may have — one cost is the expansion of data due to inserted type fields, another is the added cost of interpreting these type fields."

**ONC RPC** (RFC 1057, June 1988; updated RFC 5531, May 2009) defined the call protocol. Like Courier, it used program/version/procedure number triples. The **portmapper** (later **rpcbind**) on well-known port 111 acted as a registry — servers announced their program/version/port, and clients queried for discovery.

**rpcgen** was the IDL compiler that tied it together. It consumed `.x` files in a C-like "RPC Language" and generated:

- A header file with `#define` statements and type definitions
- XDR routines for serialization/deserialization
- Client stubs that marshal arguments and send the RPC
- A server skeleton with a dispatch loop

NFS was the killer application. NFS version 2 (RFC 1094, March 1989) used XDR for all data representation and ONC RPC for the call protocol. NFS drove ONC RPC into every Unix system and established XDR as the default binary wire format of the 1980s–90s. The `rpcgen` compiler is still shipped with Linux today, and NFSv4 still uses XDR encoding.

**Key RFCs:**

| RFC | Date | Subject |
|------|------|---------|
| 1014 | 1987 | XDR: External Data Representation Standard |
| 1057 | 1988 | ONC RPC v2 |
| 1094 | 1989 | NFS Version 2 |
| 1831 | 1995 | ONC RPC v2 (update) |
| 4506 | 2006 | XDR (Internet Standard, STD 67) |
| 5531 | 2009 | ONC RPC v2 (Internet Standard) |

### Apollo NCS and DCE/RPC (1986–1993)

While Sun was building NFS, Apollo Computer — which had the largest workstation market share in 1986 — developed the **Network Computing System (NCS)** implementing their Network Computing Architecture (NCA). NCS had three components: an RPC runtime, the **NIDL (Network Interface Definition Language)** compiler, and a **Location Broker** for service discovery. Apollo introduced the 64-bit Universal Identifier (UID), later extended to 128 bits when multi-vendor interoperability demanded a larger namespace — this is the origin of the UUID.

When the Open Software Foundation issued its "Request for Technology" in the early 1990s, Apollo's NCA/NCS (by then owned by HP, which acquired Apollo in 1989) became the foundation of **DCE/RPC**. The naming conventions survive to this day: `ncacn_tcp`, `ncacn_np`, `ncacn_http` — where "nca" is Network Computing Architecture.

DCE/RPC's wire format, **NDR (Network Data Representation)**, took the opposite philosophical stance from XDR:

| | XDR | NDR |
|---|---|---|
| Philosophy | Sender-makes-right | Receiver-makes-right |
| Byte order | Always big-endian | Sender's native order |
| Conversion cost | Sender always converts | Only converts on mismatch |
| Header overhead | None (implicit) | 4-byte format label |

NDR's format label — 4 bytes at the start of each PDU — specified integer endianness, character set (ASCII or EBCDIC), and floating-point format (IEEE, VAX, Cray, or IBM). On a homogeneous network, no conversion was needed at all. Each interface was identified by a 128-bit UUID plus a major.minor version number.

Microsoft adopted DCE/RPC wholesale. **MS-RPC** underpins DCOM, Active Directory, SMB, WMI, and much of Windows networking. **DCOM (Distributed COM)** layered "Object RPC" on top — each ORPC call was a genuine DCE RPC call where the object ID field contained an IPID (Interface Pointer Identifier). Microsoft's `midl` compiler generates stubs from IDL files in the same lineage as DCE's `idl`. NDR's wire format persists in billions of Windows machines.

### CORBA (1991–2000s)

The Object Management Group's Common Object Request Broker Architecture took the enterprise middleware path. CORBA's **CDR (Common Data Representation)** used receiver-makes-right encoding like NDR, with sender-chosen byte order flagged in the message header. Primitives were naturally aligned (2-byte shorts on 2-byte boundaries, 4-byte longs on 4-byte, etc.), and the format was implicitly typed.

CORBA's IDL was arguably its greatest contribution — it supported modules, interfaces with operations and attributes, in/out/inout parameters, exceptions, discriminated unions, sequences, multiple inheritance, and a rich set of base types. It influenced Java's interface concept, DCOM's MIDL, and many later IDLs.

But CORBA failed in practice. Michi Henning's 2006 ACM Queue article "The Rise and Fall of CORBA" cataloged the problems: API bloat (the object adapter required 200+ lines of IDL when 30 would suffice), design by committee (the OMG took the union of all competing proposals with no coherence review), the location transparency fallacy (treating local and remote objects identically made everything network-complex), and opaque object references that couldn't be constructed without a naming service. Different ORB implementations were poorly interoperable despite the "open standard" branding. CORBA's market share collapsed as Java EJB, DCOM, and eventually REST/SOAP emerged.

### OMG IDL After CORBA

CORBA's IDL outlived CORBA itself. The OMG separated it into a standalone specification — **IDL 4.0** (2018), current **IDL 4.2** (ISO/IEC 19516:2020) — and made it modular via "building blocks" that different middleware standards can select from:

- **Core Data Types**: the base type system (integers, floats, strings, structs, unions, enums, sequences)
- **Extended Data Types**: maps, bitsets, bitmasks, struct inheritance (none of these exist in protobuf)
- **Annotations**: metadata on types and fields (like Java annotations), added in IDL 4.0
- **CORBA-Specific**: legacy CORBA features (interfaces, valuetypes, exceptions with `raises`)
- **DDS-Specific**: `@topic`, `@key` annotations for Data Distribution Service

The type system is substantially richer than protobuf or Thrift: fixed-point decimals, wide strings, bitsets, bitmasks, discriminated unions with explicit discriminators, struct inheritance, and `valuetype` (pass-by-value objects with inheritance graphs, shared references, and null). OMG IDL uses C preprocessor directives (`#include`, `#define`) rather than protobuf's `import`. And critically, OMG IDL has **no field numbers** — CORBA's CDR uses positional encoding, which limits schema evolution compared to tagged-field formats.

The primary consumer of OMG IDL today is **DDS (Data Distribution Service)**, the OMG's pub/sub middleware standard used in autonomous vehicles, defense, aerospace, and financial trading. ROS 2 (Robot Operating System) uses OMG IDL via DDS. The RTPS wire protocol encodes data using CDR derived from the IDL type definitions. DDS-XTYPES extends IDL with type evolution capabilities that partially address the positional encoding limitation.

## ASN.1 — The Telecom Colossus

ASN.1 deserves its own section. It's the most powerful, most complex, and most widely deployed serialization system ever created — and almost unknown outside telecom and security.

First defined in 1984 as part of CCITT X.409, then separated into its own standard (X.208/X.209 in 1988, reorganized as X.680–X.683 in 1995), ASN.1 embodies a radical idea: **separate the abstract syntax from the transfer syntax**. You define your data structures once in ASN.1 notation, then encode them using any of seven encoding rules:

| Encoding | Standard | Year | Characteristics |
|----------|----------|------|-----------------|
| **BER** | X.690 | 1984 | Tag-Length-Value, flexible, multiple valid encodings |
| **DER** | X.690 | 1988 | Strict BER subset — exactly one encoding per value |
| **CER** | X.690 | 1988 | Canonical BER for streaming |
| **PER** | X.691 | 1994 | Bit-level packing using constraint info, most compact |
| **OER** | X.696 | 2014 | Octet-aligned, simpler than PER, more compact than BER |
| **XER** | X.693 | 2001 | XML representation |
| **JER** | X.697 | 2015 | JSON representation |

**BER** uses Tag-Length-Value encoding. The tag identifies the type (class + constructed/primitive bit + tag number), the length gives the size, and the value contains the data. DER adds restrictions — lengths must use definite form, set elements must be sorted — guaranteeing exactly one encoding per value. This property is essential for digital signatures: you must hash an unambiguous byte representation.

**PER (Packed Encoding Rules)** is where ASN.1 shows its muscle. PER eliminates TLV overhead entirely, using ASN.1 constraint information to pack data with minimal bits. An integer constrained to 0–255 takes exactly 8 bits. A boolean takes 1 bit. PER Unaligned packs at the bit level; PER Aligned pads to octet boundaries. Benchmarks show PER is competitive with Protocol Buffers on message size and often wins for deeply nested structures.

ASN.1's type system is extraordinary — constraints, parameterized types, information objects, subtype constraints, extensibility markers (`...`). The notation runs to hundreds of pages (X.680 alone). This power enables telecom standards (3GPP's LTE and 5G protocols use PER extensively), PKI (X.509 certificates use DER), SNMP (BER), LDAP (BER), Kerberos, EMV payment cards, and biometric standards.

Notable ASN.1 compilers include **asn1c** (Lev Walkin, open source), **ffasn1c** (Fabrice Bellard, supporting all encoding rules), and **ASN1SCC** (European Space Agency, targeting C and Ada for embedded/space systems).

The complexity is the barrier. Learning ASN.1 well enough to define a new protocol takes significant investment. But for the domains that use it — telecom, PKI, network management — nothing else comes close to the combination of compactness, precision, and multi-encoding flexibility.

## The XML Interlude (1998–2010)

The late 1990s brought a brief detour into text-based RPC.

**XML-RPC** (1998), created by Dave Winer at UserLand Software with Microsoft collaboration, was deliberately simple: HTTP POST with XML payloads, a handful of types (`i4`, `boolean`, `string`, `double`, `dateTime.iso8601`, `base64`, plus `struct` and `array`), and fault codes for errors. It worked. It was easy to debug (just read the XML). And it was enormous — an integer that took 4 bytes in XDR might take 50 bytes in XML-RPC.

**SOAP** evolved from XML-RPC through corporate politics. Winer shipped XML-RPC independently when Microsoft internal disputes delayed the full SOAP specification. SOAP 1.0 arrived in late 1999 from DevelopMentor and Microsoft; SOAP 1.2 became a W3C Recommendation in 2003. SOAP added envelope structure, header blocks for extensibility, encoding rules, and a binding framework. **WSDL** (Web Services Description Language) served as the IDL, filling the same role as rpcgen's `.x` files.

Then came the WS-\* explosion: WS-Security, WS-ReliableMessaging, WS-AtomicTransaction, WS-Addressing, WS-Policy, WS-Federation, WS-Trust, WS-SecureConversation, WS-Coordination... The WS-I (Web Services Interoperability Organization) produced "massively complex layered specifications" that were widely criticized as bloated. REST emerged as the reaction — leveraging HTTP's existing infrastructure instead of building a parallel universe atop it.

### EXI — Trying to Save XML (2011)

The W3C's **Efficient XML Interchange (EXI)** was an attempt to give XML binary-format performance. Published as a W3C Recommendation in March 2011, EXI uses **grammar-driven encoding**: it defines state machines for XML document structure, where each XML event (start element, attribute, character data, end element) is mapped to an **event code** — an n-bit integer where n = ceil(log₂(number of alternatives)). Frequently occurring events get shorter codes.

In **schema-informed mode**, XML Schema constraints dramatically reduce event code bit-widths. If only one element can appear in a given context, its event code is 0 bits — free. String tables index repeated strings so subsequent occurrences use compact integer indices. The result is impressive: over 100x smaller than text XML in some cases, up to 14x more compact than gzipped XML.

EXI found adoption in constrained environments where XML was mandated by existing standards — IoT sensor networks, XMPP stream compression (XEP-0322), smart grid protocols (IEC 61850), and the W3C's Web of Things. Implementations exist in Java (OpenEXI, EXIficient) and C (EXIP, targeting embedded systems).

But EXI was solving a problem the industry was already abandoning. JSON replaced XML for most new APIs, and binary formats like protobuf and CBOR replaced both for performance-critical use. EXI 2.0 was in development, but the working group's charter expired. EXI remains relevant only where XML is mandated by legacy standards.

The lesson: human readability is valuable for debugging but expensive at scale. The 10–30x size overhead of XML over binary formats is untenable for high-throughput systems. And complexity layered atop complexity collapses under its own weight.

## The Modern Binary Era

### Protocol Buffers (2001/2008)

Google developed Protocol Buffers internally starting around 2001 ("Proto1"), with Proto2 as a clean rewrite. The format was open-sourced in 2008, and by then it was already the dominant internal data interchange format at Google — used in virtually every structured data exchange.

Protobuf's wire format is elegant. Each field is encoded as a **varint tag** followed by the value. The tag encodes `(field_number << 3) | wire_type` — six wire types cover all cases:

| Wire Type | Meaning | Used For |
|-----------|---------|----------|
| 0 | Varint | int32, int64, uint32, uint64, sint32, sint64, bool, enum |
| 1 | 64-bit | fixed64, sfixed64, double |
| 2 | Length-delimited | string, bytes, embedded messages, packed repeated fields |
| 3 | Start group | groups (deprecated) |
| 4 | End group | groups (deprecated) |
| 5 | 32-bit | fixed32, sfixed32, float |

Varints use 7 payload bits per byte with 1 continuation bit (MSB), in little-endian byte order within the varint. Field numbers 1–15 encode in a single tag byte, 16–2047 in two bytes — motivating the convention of reserving low numbers for frequently-used fields. Signed integers use **ZigZag encoding** (`sint32`/`sint64`) to efficiently encode small negative values as small varints.

The `.proto` IDL supports messages, enums, oneofs (discriminated unions), maps, nested types, and service definitions. Proto3 simplified the language (removing required/optional distinction, always using default values for absent fields). A newer "Editions" syntax is replacing the proto2/proto3 split.

Schema evolution works through field numbers: fields can be added or removed as long as numbers aren't reused. Unknown fields are preserved on decode. This is the key innovation over XDR — a proto message can be read by a program compiled against an older or newer version of the schema.

**Language support:** C++, C#, Dart, Go, Java, Kotlin, Objective-C, Python, Rust, Ruby, PHP (official); many more via the `protoc` plugin system.

### Apache Thrift (2006/2007)

Facebook created Thrift in 2006 because Protocol Buffers wasn't yet open-sourced. Published as a Facebook technical paper in April 2007 and donated to the Apache Foundation in 2010, Thrift is a complete RPC stack — not just serialization.

Thrift offers multiple wire protocols:

- **TBinaryProtocol**: straightforward big-endian binary with field type byte + 16-bit field ID + value
- **TCompactProtocol**: varint encoding with ZigZag for signed integers and a clever optimization — if consecutive field IDs have a delta less than 15, the delta is packed into the type byte's upper 4 bits, eliminating the 2-byte field ID entirely
- **TJSONProtocol**: JSON encoding

The `.thrift` IDL supports structs, unions, exceptions, enums, services with inheritance, typedefs, constants, and namespaces. Schema evolution works through numeric field IDs, like protobuf.

Thrift supports an impressive range of languages: C, C++, C#, D, Dart, Delphi, Erlang, Go, Haskell, Java, JavaScript, Kotlin, Lua, OCaml, Perl, PHP, Python, Ruby, Rust, Smalltalk, Swift, and more.

In practice, TCompactProtocol performs nearly identically to protobuf on both size and speed. The choice between Thrift and protobuf is more about ecosystem than format — protobuf has gRPC, Thrift has broader language support out of the box.

### Apache Avro (2009)

Doug Cutting — creator of Hadoop — designed Avro specifically to address limitations of protobuf and Thrift for the Hadoop ecosystem. The key insight: **schema resolution by name, not by number**.

Avro's wire format contains **no per-field tags or IDs**. Field order is determined entirely by the schema. Scalars use varints (ZigZag-encoded), strings are length-prefixed UTF-8, arrays and maps use a block structure (count + items, terminated by zero-count), and unions use a varint branch index.

The **Object Container File** format embeds the writer's schema as JSON in the file header, followed by data blocks separated by 16-byte random sync markers. This design enables MapReduce splitting (start processing at any sync marker) and makes files self-describing — you can always read an Avro file if you have an Avro library, even years later.

Schema evolution works differently from protobuf: the reader's schema and writer's schema can differ, and Avro resolves fields by name. This means fields can be added (with defaults), removed, or have their types promoted — without coordinating numeric IDs. The trade-off is that Avro requires the schema to be available at read time, not just compile time.

Avro is the dominant format in the Hadoop ecosystem. Apache Kafka's Schema Registry uses Avro schemas as the default. For data lake and pipeline use cases where data must remain readable across years and many schema versions, Avro's approach is compelling.

### gRPC (2015)

Google's gRPC replaced their internal "Stubby" system (in use since ~2001) with an open-source framework built on Protocol Buffers and HTTP/2. Released in August 2016, gRPC is now a CNCF graduated project.

The wire format layers protobuf over HTTP/2's binary framing: each gRPC message is a 1-byte compressed flag + 4-byte message length + serialized protobuf payload. HTTP/2 provides multiplexed streams over a single TCP connection, header compression (HPACK), and flow control. gRPC defines four RPC patterns: unary, server streaming, client streaming, and bidirectional streaming.

gRPC's strength is the ecosystem: load balancing, service mesh integration (Istio/Envoy), interceptors, deadline propagation, health checking, and reflection services. The weakness is browser support — HTTP/2's binary framing doesn't work from browsers without a grpc-web proxy.

(The "g" in gRPC stands for a different word in each release — "good," "green," "groovy," etc.)

## Zero-Copy Formats

The zero-copy formats take a different approach: instead of encoding data into a compact wire format and then decoding it back into in-memory objects, they define the wire format to *be* the in-memory representation. Reading a field means doing pointer arithmetic on the serialized buffer — no parsing step, no memory allocation. The trade-off is that your application works with the format's accessor objects instead of native language types, which has real ergonomic and performance implications depending on the access pattern.

### FlatBuffers (2014)

Created by Wouter van Oortmerssen at Google, FlatBuffers was originally designed for mobile game development where allocation and copy costs matter. The core data structure is the **vtable** (virtual table) — a table of offsets to each field in a record, shared and deduplicated across instances with identical layouts.

Building a FlatBuffer works back-to-front: inner objects first, then containers, then the root. Strings are null-terminated UTF-8 with length prefix. Absent fields have default values (stored in the schema, not the buffer), so they take zero space. The format uses three offset types: `uoffset_t` (unsigned, forward references), `soffset_t` (signed, vtable references), and `voffset_t` (16-bit, within vtable).

The trade-off: FlatBuffers are larger than protobuf (due to alignment padding and vtables) but infinitely faster to read — there's nothing to decode. Schema evolution works through vtable indirection: new fields get new vtable slots, old readers simply don't access them.

**FlexBuffers**, a schemaless companion format, uses automatic width optimization (8/16/32/64-bit values sized to minimum necessary) and string deduplication. Despite being schemaless, FlexBuffers often produces smaller output than schema-based FlatBuffers.

### Cap'n Proto (2013)

Created by Kenton Varda — the primary author of Protocol Buffers v2 at Google — Cap'n Proto takes zero-copy to its logical extreme. Varda's tagline: "serialization is a lie." The idea is that the in-memory representation IS the wire format. You can `mmap` a Cap'n Proto file and start reading immediately; the OS only loads the pages you access.

The fundamental unit is an 8-byte word. Messages consist of segments (contiguous byte arrays) preceded by a segment table. Pointers are 64-bit values encoding struct pointers (offset + data section size + pointer count), list pointers, far pointers (cross-segment), and capability pointers (for RPC). Default values are stored in the schema, not the data — an absent field reads as default with zero storage cost.

The schema language uses `@N` annotations for field numbering (like protobuf's field numbers but explicitly annotated). Cap'n Proto also includes an RPC system with promise pipelining — you can chain calls without waiting for intermediate results, and the system batches the round trips.

Varda created Cap'n Proto because he saw Google burning massive CPU on protobuf encode/decode. Cap'n Proto is used in Cloudflare Workers (Varda works at Cloudflare) and other latency-sensitive systems.

**The "no serialization" claim deserves scrutiny.** Cap'n Proto's website says "there is no encoding/decoding step," but in practice it ships a **packer** that deflates runs of zero bytes (an RLE variant) to bring wire sizes close to protobuf. Unpacked, the 8-byte-word-aligned format is substantially larger. So you still have an encode/decode step for anything going over a network or to disk — it's just called "packing" instead of "serialization."

More fundamentally, "zero-copy" means you don't work with your language's native types. You work with Cap'n Proto's accessor objects that do pointer arithmetic into the serialized buffer. In C++ this means giving up structs, standard containers, and normal memory management in favor of arena-allocated Cap'n Proto builders and readers. In higher-level languages the ergonomic cost is even steeper. You've traded serialization overhead for the overhead of never having native objects in the first place — every field access goes through an indirection layer. Whether this is a net win depends entirely on the access pattern. If you read a few fields from a large message and discard the rest, zero-copy is genuinely faster. If you need to process most fields, or hold the data in memory for computation, you end up copying into native types anyway and the "zero-copy" advantage evaporates. The same critique applies to FlatBuffers, though FlatBuffers is more upfront about the trade-off.

### SBE (Simple Binary Encoding, ~2013)

SBE was designed by the FIX Trading Community's High Performance Work Group with a reference implementation by Real Logic (Martin Thompson). It targets financial trading systems where latency is measured in nanoseconds.

SBE uses **fixed-layout messages**: fields at fixed offsets, no tags or length prefixes for fixed fields. A message header gives block length, template ID, schema ID, and version. Variable-length fields are allowed only at the end, preserving streaming semantics. The critical constraint: **no backtracking during encode or decode**. This enables hardware prefetching and eliminates branch mispredictions.

Benchmarks show SBE achieving 16–25x higher throughput than protobuf for typical financial messages, encoding/decoding in tens of nanoseconds. The format is designed to operate entirely within L1 cache. Schema evolution is limited to append-only fields with version tracking.

## Schemaless Binary Formats

Not every use case needs a schema. For dynamic data, configuration, or protocols where both sides don't share compiled schemas, schemaless binary formats offer a middle ground between JSON's verbosity and protobuf's rigidity.

### MessagePack (2008)

Created by Sadayuki Furuhashi, MessagePack is essentially "binary JSON" — a compact binary encoding of the same data model (maps, arrays, strings, integers, floats, booleans, nil). The first byte determines the type and sometimes contains the value: small positive integers (0–127) encode as a single byte, short strings (up to 31 bytes) use a 1-byte header, and short arrays/maps (up to 15 elements) likewise.

The result is roughly 50% smaller than JSON for typical data. MessagePack adds binary data (unlike JSON's string-only model) and extension types for user-defined data, and allows any type as a map key. Over 50 language implementations exist. Redis, Fluentd, and many embedded systems use it.

### BSON (2009)

MongoDB's BSON (Binary JSON) was designed not for compactness but for **traversability**. Every document starts with a 4-byte (little-endian) total length. Elements are encoded as type byte + null-terminated C-string field name + value. This design enables skip-ahead traversal — you can jump to the next field without parsing the current one's value.

BSON can actually be *larger* than JSON for some documents due to length prefixes and type tags. But it supports types JSON lacks: ObjectId (12-byte, including timestamp/machine/PID/counter), UTC datetime, binary data, 128-bit decimal, regex, and JavaScript code. The little-endian byte order (unlike XDR's big-endian) reflects MongoDB's preference for x86 performance.

### CBOR (2013/2020)

CBOR (Concise Binary Object Representation, RFC 8949) was designed by the IETF for constrained environments — IoT devices, embedded systems, anything where code size matters. A CBOR decoder can be written in under 1 KB of code.

The encoding is clean: each data item starts with one byte where the high 3 bits give the major type (0–7: unsigned int, negative int, byte string, text string, array, map, tag, simple/float) and the low 5 bits give additional information. Short values (0–23) encode in the initial byte itself. Big-endian byte order. Indefinite-length strings and arrays support streaming.

CBOR's **tag system** is its distinguishing feature — tags attach semantic meaning (datetime, bignum, URI, base64, etc.) via an IANA-managed registry. COSE (CBOR Object Signing and Encryption) and CWT (CBOR Web Tokens) are the CBOR equivalents of JOSE/JWT. WebAuthn/FIDO2 uses CBOR. The COAP protocol for constrained networks uses CBOR payloads.

### Amazon Ion (2016)

Ion is Amazon's dual-format serialization: text and binary representations that are fully interchangeable. The text format is a strict superset of JSON — all valid JSON is valid Ion — which makes migration painless. Ion adds types JSON lacks: blob, clob, timestamp (ISO 8601), symbol, decimal (arbitrary precision), and s-expressions.

The binary format uses a symbol table system: string symbols are replaced with integer IDs, amortized over a stream (similar to how class loading works in VMs). Ion Schema Language (ISL) provides optional validation. Amazon uses Ion in DynamoDB exports and QLDB (Quantum Ledger Database), where Ion's deterministic encoding enables cryptographically verifiable ledger entries.

## Columnar Formats — Serialization for Analytics

Row-oriented serialization (protobuf, Thrift, Avro) stores each record contiguously: all fields of record 1, then all fields of record 2. This is natural for transactional workloads where you read or write one record at a time. But for analytics — "compute the average of column X across 10 million rows" — row-oriented formats waste bandwidth reading every column just to access one.

Columnar formats flip the layout: all values of column 1, then all values of column 2. This enables **column pruning** (skip columns you don't need), **vectorized processing** (operate on an entire column with SIMD instructions), and far better compression (similar values cluster together).

### Apache Arrow (2016)

Apache Arrow, created by Wes McKinney (creator of pandas) and collaborators, is not a file format — it's an **in-memory columnar format** and cross-language development platform. Accepted directly as an Apache Top-Level Project in February 2016 (bypassing incubation), Arrow defines how columnar data should be laid out in RAM.

The memory layout is precise:

- **Primitive arrays** (Int32, Float64, etc.): a **validity bitmap** (1 bit per value, 0 = null) plus a contiguous buffer of fixed-width values
- **Variable-length arrays** (strings, binary): validity bitmap + offsets buffer (N+1 int32 offsets) + values buffer (concatenated bytes)
- **Nested types**: struct arrays combine child arrays; list arrays use an offsets buffer pointing into a child array
- All buffers aligned to 8-byte boundaries (64-byte recommended for SIMD)
- Null values consume space in the data buffer but are masked by the validity bitmap

A **record batch** — a collection of equal-length arrays plus a schema — is the fundamental unit of data exchange. Schemas are defined via FlatBuffers metadata.

The core insight is that **the on-disk representation IS the in-memory representation**. Arrow IPC (also called Feather v2) stores record batches in two modes: streaming (sequential, for pipes/sockets) and file (random access, with a footer of offsets). Memory-mapping an Arrow IPC file gives zero-copy access with no deserialization.

**Arrow Flight** is an RPC protocol built on gRPC that transmits Arrow record batches directly over the network. Protobuf is used only for metadata/control messages; actual data payloads are raw Arrow IPC format, bypassing protobuf serialization entirely. Flight supports parallel partitioned reads — the server can return multiple endpoints for a dataset.

Arrow has implementations in 13 languages (C, C++, C#, Go, Java, JavaScript, Julia, MATLAB, Python, R, Ruby, Rust, Swift). If two systems both use Arrow in-memory format, data transfer is a pointer handoff or raw memory copy — no encode/decode step. This is why pandas 2.0+ supports ArrowDtype as a backend, DuckDB can query Arrow tables zero-copy, and Polars is built natively on Arrow's memory format.

The limitation: Arrow is a transport/compute format, not a storage format. It has no built-in compression, no schema evolution semantics, and no predicate pushdown. For persistent storage, you want Parquet.

### Apache Parquet (2013)

Parquet is the columnar *storage* format that complements Arrow's in-memory layout. Created jointly by Twitter (Julien Le Dem) and Cloudera (Nong Li), Parquet was inspired by Google's 2010 Dremel paper on nested columnar storage.

A Parquet file is hierarchical:

- **File** → one or more **row groups** (horizontal partitions, typically 128 MB–1 GB each)
- **Row group** → one **column chunk** per column (guaranteed contiguous in the file)
- **Column chunk** → one or more **pages** (the indivisible unit of compression/encoding, typically ~1 MB)

Pages use encoding schemes chosen per-column:

| Encoding | Technique |
|----------|-----------|
| **RLE_DICTIONARY** | Dictionary page of unique values + integer indices with RLE/bit-packing |
| **DELTA_BINARY_PACKED** | Delta encoding for integers |
| **DELTA_BYTE_ARRAY** | Incremental/prefix encoding for byte arrays |
| **BYTE_STREAM_SPLIT** | Scatters K bytes of each float across K streams (exploits IEEE 754 patterns) |
| **PLAIN** | Raw unencoded values |

Compression (Snappy, GZIP, Brotli, ZSTD, LZ4) is applied per-page on top of encoding.

Nested data uses Dremel's **record shredding**: nested structures are decomposed into flat columns with **definition levels** (how many optional fields in the path are defined) and **repetition levels** (which repeated field caused a new value). This is the key contribution from the Dremel paper — it makes arbitrary nesting work in a columnar layout.

The file footer (Thrift-serialized) contains schema, column statistics (min/max, null count, distinct count), and offsets. This enables **predicate pushdown** — skip entire row groups whose min/max stats don't match a filter — and **column pruning** — fetch only the column chunks for requested columns.

Arrow and Parquet are complementary: Parquet for storage (compressed, encoded, with statistics), Arrow for compute (uncompressed, SIMD-friendly, zero-copy). The typical pipeline: read Parquet from disk → decode into Arrow record batches → compute. Arrow libraries include optimized Parquet readers/writers.

## IPC Formats

### D-Bus (2002/2006)

D-Bus is the standard IPC mechanism for Linux desktops and systemd-based systems, designed to replace CORBA (GNOME's original IPC) and DCOP (KDE's). Created by Havoc Pennington, Alex Larsson, and Anders Carlsson at Red Hat under the freedesktop.org project, D-Bus 1.0 was released in November 2006.

The wire protocol is a binary format with per-message byte order (first byte: `'l'` for little-endian, `'B'` for big-endian). Messages have a fixed header structure: byte order, message type (METHOD_CALL, METHOD_RETURN, ERROR, SIGNAL), flags, protocol version, body length, serial number, and an array of typed header fields. The header is padded to an 8-byte boundary before the body.

The type system uses single-character **type signatures** in a compact grammar:

- **Basic types**: `y` (byte), `b` (boolean, 4 bytes!), `n`/`q` (int16/uint16), `i`/`u` (int32/uint32), `x`/`t` (int64/uint64), `d` (double), `s` (string), `o` (object path), `g` (signature), `h` (Unix FD)
- **Container types**: `a` (array), `(...)` (struct), `v` (variant — carries its own signature), `{...}` (dict entry, only inside arrays)
- Maximum nesting: 32 levels

Marshalling uses natural alignment: 2-byte types on 2-byte boundaries, 4 on 4, 8 on 8. Structs always start on 8-byte boundaries regardless of contents. Strings are UTF-8 with uint32 length prefix and NUL terminator. Maximum message size: 128 MB.

D-Bus uses a central **message broker** (dbus-daemon, or the faster **dbus-broker** used by default in Fedora/RHEL). All messages route through this broker, adding latency — typical method call round trips are 0.3–1 ms, slow by modern IPC standards. The **kdbus** project (2013–2015) attempted a kernel-based D-Bus implementation but failed to merge into Linux due to controversy over protocol-specific kernel code. **bus1**, a more generic successor, also never merged.

Implementations include the reference **libdbus** (C), **sd-bus** (systemd, simpler API), **GDBus** (GLib/GNOME), **QtDBus** (Qt/KDE), and **zbus** (Rust). D-Bus is used by NetworkManager, PulseAudio, GNOME Shell, KDE, systemd, BlueZ, and essentially every Linux desktop service. Two buses exist: session (per-user) and system (system-wide).

## JIT-Compiled Serialization

### Apache Fory (2019/2023)

Apache Fory (originally "Fury," renamed for ASF branding) represents a different approach: instead of designing a clever wire format, throw runtime code generation at the problem. Created by Chaokun Yang at Ant Group (Alibaba subsidiary) in 2019, inner-sourced in 2022, open-sourced July 2023, and graduated to an Apache Top-Level Project in September 2025.

Fory's Java implementation uses **JIT compilation at runtime** — it generates optimized serialization code that eliminates virtual method calls and inlines hot paths. For other languages (Rust, C++, Go), it uses static code generation. The result: claims of up to 170x faster serialization than competing frameworks in their benchmarks.

The wire format uses a 2-byte magic number (`0x62d4`, little-endian) and operates in three modes:

- **Native mode**: single-language, no type metadata overhead (fastest)
- **Xlang mode**: cross-language with full type information
- **Compatible mode**: ClassDef metadata per struct for schema evolution (forward/backward compatible)

Fory preserves shared and circular references across languages and supports polymorphism — actual runtime types survive serialization, unlike protobuf where everything flattens to the schema type. Language support: Java (primary), Python, Go, JavaScript, Rust, C++.

Ant Group reports Ant Ray uses Fory across 1M+ CPU cores daily, with 2.5x TPS improvement for task scheduling and 4x for data transfer after switching from their previous serialization. Binary compatibility across major versions is not yet guaranteed — that's promised from 1.0 onward.

## Simple Framing Formats

### Netstrings (1997)

Daniel J. Bernstein's netstrings solve exactly one problem — self-delimiting byte strings — and solve it perfectly:

```
<length in ASCII digits>:<data>,
```

"hello world!" becomes `12:hello world!,`. Empty string becomes `0:,`. That's the entire specification.

No escape sequences, no ambiguity, trivial to implement in any language, composable (netstrings can contain netstrings). Netstrings avoid all the complications of delimiter-based framing: arbitrary binary data works, you know exactly how many bytes to read, there's no buffering problem. Used in SCGI, QMQP, and the broader djb software ecosystem.

**TNetstrings** extended the concept with a type tag: `5:hello,s` for string, `3:123,i` for integer.

### Bencode (2001)

Bram Cohen's Bencode, the wire format of the BitTorrent protocol, uses the same length-prefix idea for strings (`4:spam`) but adds `i<number>e` for integers, `l...e` for lists, and `d...e` for sorted-key dictionaries. The type system is deliberately tiny: integers (arbitrary precision, base-10), byte strings, lists, dictionaries. No floats, no null, no booleans.

The critical property is **canonical encoding** — dictionary keys must be sorted, and there is exactly one valid encoding per value. This makes Bencode ideal for content addressing: `.torrent` files are hashed to produce `info_hash`, and deterministic encoding ensures identical data always produces identical hashes. BEP 5 uses Bencode for Kademlia DHT messages over UDP. BEP 44 extends this to storing and retrieving arbitrary Bencode data in the DHT — immutable items keyed by SHA-1 hash, mutable items keyed by Ed25519 public key.

Outside BitTorrent, Bencode has seen minimal adoption. The format is too limited for general use, but its canonical encoding property is valuable for any protocol that needs to hash or sign structured data.

### S-Expressions (1958)

S-expressions may be the oldest serialization format still in active use. John McCarthy introduced them in 1958 for Lisp, and they've never stopped being useful.

Ron Rivest formalized **canonical S-expressions** for SPKI/SDSI cryptographic certificates in 1997, finally published as RFC 9804 in June 2025. Rivest defined three representations: canonical (unique binary form for cryptographic hashing, using netstring-style `<length>:<data>` atoms), basic transport (base64-wrapped), and advanced (human-readable with display hints). The canonical form guarantees unique encoding — critical for digital signatures, just as Bencode's canonical form is critical for BitTorrent hashes.

Modern non-Lisp uses are more widespread than most people realize. **KiCad** (v4+, 2013) uses S-expressions for all file formats — schematics (`.kicad_sch`), PCB layouts (`.kicad_pcb`), symbol and footprint libraries. This is probably the largest non-Lisp deployment of S-expressions. **WebAssembly's text format (WAT)** uses S-expressions for module definitions. **GNU Guix** uses S-expressions (via Guile Scheme) for all package definitions. GnuPG and libgcrypt use Rivest's canonical S-expressions for key storage.

### Plan 9 9P (1990s)

The Plan 9 filesystem protocol is a masterclass in minimalism: 14 message types total. Wire format: little-endian, length-prefixed messages. 4-byte total size, 1-byte message type (T-messages for requests, R-messages for responses), 2-byte tag for request/response matching. Strings are 2-byte length prefix + UTF-8, no NUL terminator. Everything is a file — the protocol handles all resource access through walk/open/read/write/clunk operations. Modern systems use 9P via virtio-9p for VM file sharing.

## The Oldest Survivor — CSV

CSV predates every format in this article. The earliest known use dates to 1972, with IBM's Fortran level H extended compiler under OS/360 supporting list-directed I/O with comma delimiters. FORTRAN 77 (1978) standardized list-directed I/O. The term "CSV" was coined around 1983. RFC 4180 (October 2005) attempted to standardize the format — as an Informational RFC, not a full standard.

CSV has no schema, no types, no nesting. Everything is a string. There is no way to distinguish integer `42` from string `42` without external knowledge. And yet CSV is immortal — universally supported by spreadsheets, databases, and every programming language.

The problems are legendary:

- **Encoding**: no standard encoding. Could be Latin-1, UTF-8, Windows-1252, Shift-JIS. Excel requires a UTF-8 BOM to recognize UTF-8; other tools choke on it.
- **Delimiter**: comma in US/UK, semicolon in locales where comma is the decimal separator (France, Germany). TSV uses tabs.
- **Quoting**: fields containing commas, quotes, or newlines must be double-quoted. Quotes inside quoted fields are escaped by doubling (`""`). Newlines inside quoted fields are legal but many parsers break on them.
- **Newlines**: RFC says CRLF. The real world uses LF, CR, or CRLF.
- **No null**: an empty field could mean null, empty string, or zero.
- **Formula injection**: fields starting with `=`, `+`, `-`, `@` are interpreted as Excel formulas — a real security issue.
- **Leading zeros**: Excel silently strips them (`00123` becomes `123`).

CSV persists because it's the simplest possible tabular format. No library is needed for basic cases. Excel opens it natively. Every database can export it. For better or worse, it's the universal common denominator for tabular data interchange.

## The Encoding Philosophy Spectrum

The formats above can be arranged along a spectrum of encoding philosophy:

| System | Byte Order | Typing | Alignment | Philosophy |
|--------|-----------|--------|-----------|------------|
| XDR | Big-endian (fixed) | Implicit | 4-byte | Sender converts to canonical |
| NDR | Sender's native | Implicit | Natural | Receiver converts if needed |
| CDR (CORBA) | Sender's native | Implicit | Natural | Receiver converts if needed |
| ASN.1 BER | N/A (TLV) | Explicit tags | Octet | Self-describing |
| ASN.1 PER | Bit-packed | From schema | Bit or octet | Schema-driven minimal encoding |
| Protobuf | Little-endian varints | Tagged fields | None | Compact tagged fields |
| Cap'n Proto | Little-endian | Implicit + vtable | 8-byte word | Zero-copy, in-place access |
| FlatBuffers | Little-endian | Vtable offsets | Natural | Zero-copy, vtable indirection |
| SBE | Native | Fixed offsets | Fixed layout | Zero-copy, fixed layout |
| Arrow | Little-endian | Schema in metadata | 8/64-byte | Columnar zero-copy |
| Parquet | Little-endian | Schema in footer | Page-level | Columnar compressed storage |
| D-Bus | Per-message | Signature string | Natural | IPC with type signatures |
| CBOR | Big-endian | Self-describing | Octet | Compact self-describing |
| Fory | Little-endian | Runtime/JIT | None | JIT-compiled native speed |
| Netstrings | N/A (text length) | None | None | Minimal framing only |
| Bencode | N/A (text) | Self-delimiting | None | Canonical encoding for hashing |
| 9P | Little-endian | Implicit | Packed | Simplicity |
| CSV | N/A (text) | None | None | Universal lowest-common-denominator |

The "sender-makes-right" vs "receiver-makes-right" debate is the oldest in the field. XDR chose a single canonical form for simplicity; NDR argued that on homogeneous networks (the common case), you waste work converting to a canonical form and back. Both were right for their era. Today, with x86/ARM dominance and little-endian near-universal, the question matters less — but protobuf's choice of little-endian varints and Cap'n Proto's little-endian words reflect the modern reality.

## Schema vs Schemaless

The choice between schema-based and schemaless formats is one of the most consequential architectural decisions in distributed systems.

**Schema-based formats** (protobuf, Thrift, Avro, FlatBuffers, Cap'n Proto, SBE, ASN.1) produce smaller wire sizes (no type/field name info per value), provide compile-time type safety, include built-in schema evolution rules, and enable code generation for optimized serializers. The cost is upfront schema definition and management.

**Schemaless formats** (JSON, MessagePack, CBOR, BSON, Ion, FlexBuffers, Bencode) are self-describing, flexible, and require no schema coordination between producer and consumer. Prototyping is faster. But wire sizes are larger, and validation must be done at the application level.

**Hybrid approaches** exist: Avro embeds schemas in container files but not individual records. CBOR has CDDL for optional validation. JSON Schema validates documents against a schema. Ion has ISL. These offer some schema benefits without the full compile-time commitment.

For high-throughput services, the schema overhead pays for itself quickly. For configuration files, debugging, and ad-hoc data exchange, schemaless wins on developer experience.

## Performance Comparison

Performance varies by workload, but the general hierarchy is consistent across benchmarks:

| Tier | Formats | Encode/Decode | Wire Size |
|------|---------|---------------|-----------|
| Zero-copy | SBE, Cap'n Proto, FlatBuffers, Arrow IPC | Nanoseconds (no decode step) | Medium-large (alignment padding) |
| JIT-compiled | Fory (native mode) | Nanoseconds–microseconds | Small-medium |
| Tagged binary | Protobuf, Thrift Compact | Microseconds | Smallest |
| Schemaless binary | MessagePack, CBOR | Microseconds | Small-medium |
| Self-describing | BSON, Ion binary | Microseconds | Medium |
| Columnar storage | Parquet, Avro (container) | Microseconds (with column pruning) | Smallest (with encoding + compression) |
| Text | JSON, XML, CSV, SOAP | Milliseconds | Large-very large |

Notable benchmark suites:

- **thekvs/cpp-serializers** — C++ comparison across formats
- **djkoloski/rust_serialization_benchmark** — Rust benchmarks
- **FlatBuffers official benchmarks** at flatbuffers.dev
- **SBE benchmarks** showing 16–25x throughput over protobuf for financial messages

The zero-copy formats dominate on decode latency but may produce larger serialized output due to alignment requirements. Protobuf and Thrift Compact produce the smallest payloads for most workloads. For the vast majority of applications, the difference between protobuf and any schemaless binary format is negligible — the real gains come from avoiding text formats at high throughput.

## Language-Native Serialization — A Cautionary Tale

Several programming languages ship built-in serialization that directly marshals native objects:

| Format | Language | Year | Status |
|--------|----------|------|--------|
| Java Serialization | Java | 1997 | Deprecated in practice |
| Pickle | Python | ~1999 | Active but dangerous |
| PHP serialize | PHP | ~2000 | Active but dangerous |
| Marshal | Ruby | — | Active but dangerous |
| .NET BinaryFormatter | C# | 2002 | **Removed in .NET 9 (2024)** |

These formats share a fatal flaw: they can instantiate arbitrary classes during deserialization, enabling remote code execution. Java's `ObjectInputStream` is the most notorious source of deserialization vulnerabilities in computing history — the 2017 Equifax breach exploited Java deserialization via Apache Struts. Python's `pickle` uses a stack-based virtual machine where `__reduce__` can return arbitrary callable + args. PHP's `unserialize()` enables object injection via class autoloading.

Mark Reinhold (Java's chief architect) has called serialization "a horrible mistake." Josh Bloch called it "Java's biggest mistake." Microsoft removed BinaryFormatter from .NET 9 entirely after years of unfixable security issues first documented at Black Hat in 2012.

The lesson is clear: **never deserialize untrusted data using language-native serialization**. Use schema-based binary formats (protobuf, Thrift, Avro, FlatBuffers, Cap'n Proto, CBOR, MessagePack) — they produce data values only, never executable code.

## Other Notable Formats

Several other formats deserve mention:

**Erlang External Term Format (ETF, 1997)** directly represents Erlang's type system, including PIDs, references, and ports. Used for all inter-node Erlang distribution and by Elixir. **BERT** (2009, Tom Preston-Werner) was a cross-language adaptation used at GitHub.

**Hessian** (Caucho Technology) is a compact binary RPC protocol that reads like a bytecode switch statement. Handles arbitrary object graphs with circular references. Widely used in Apache Dubbo (Chinese microservices ecosystem).

**Smile** (2010, FasterXML/Jackson) is a binary JSON format whose header starts with `:)\n`. Back-references eliminate redundant strings, achieving 30–50% size reduction over JSON.

**Microsoft Bond** (open-sourced 2015) supported generics and inheritance in its IDL — richer than protobuf. Used in Bing, Office 365, and Azure. **Ended March 31, 2025** — no further development.

**Kryo** (~2010) is a Java-specific framework widely used in Spark, Flink, Storm, and Akka. Pluggable serializer architecture, much faster than Java's built-in serialization.

**ZeroMQ (ZMTP)** is not a serialization format but a transport framing protocol. ZMTP messages consist of frames: length field + flags byte + body. The multipart message concept (multiple frames composing a logical message) is unique. Born from iMatix's frustration with AMQP's complexity — Pieter Hintjens announced iMatix would leave the AMQP workgroup in 2010 in favor of ZeroMQ's "zero broker, zero latency, zero administration" philosophy.

## How to Choose

The space of serialization formats is large, but the practical decision tree is not:

**Cloud microservices**: gRPC (protobuf + HTTP/2). The ecosystem — load balancing, service mesh, interceptors, deadline propagation — matters more than raw format performance. Thrift is a solid alternative with broader language support.

**Big data / data lakes**: Parquet for storage (columnar encoding, compression, predicate pushdown), Arrow for compute (zero-copy in-memory, SIMD-friendly). Avro for streaming pipelines and Kafka (schema-in-file, schema evolution by name).

**Latency-critical systems (trading, games, embedded)**: SBE for fixed-layout financial messages, FlatBuffers for games and Android internals, Cap'n Proto for IPC where you want zero-copy with schema evolution. Fory if you're in the JVM ecosystem and want JIT-compiled speed with Java-native object graphs.

**IoT / constrained devices**: CBOR. IETF standard, tiny decoder footprint, semantic tags, COSE/CWT for security.

**Telecom / PKI**: ASN.1 with PER (for compactness) or DER (for signatures). Nothing else has the standard body support or the installed base.

**Web APIs**: JSON. The universal common denominator. When performance matters, MessagePack or CBOR as binary alternatives with the same data model.

**Simple framing**: Netstrings. If all you need is length-prefixed byte strings, don't reach for anything more complex.

**Cross-language configuration / interchange**: Ion (if in the Amazon ecosystem), CBOR (if standard-body backing matters), MessagePack (for maximum language support).

## Demo: Building a Tiny IDL Compiler

All the formats above share a common pattern: define a schema, generate code, encode tagged fields. To make this concrete, the accompanying demo builds a minimal serialization system from scratch — an IDL file, a shell+awk code generator, and a C runtime — in about 200 lines total.

### What Are Tag Numbers?

If you've only worked with JSON or CSV, the idea of "tag numbers" may not be obvious. In JSON, field identity is carried by the field *name* — the string `"temperature"` appears on the wire every time you send a temperature value. That's human-readable but wasteful: 11 bytes of name for a 2-byte value.

Tag numbers replace field names with small integers assigned in the schema. The tag number is encoded on the wire instead of the name. Both sides have the schema, so they know that tag 4 means "temperature" — the name never appears in the serialized data.

This has two benefits. First, it's compact: a tag number fits in a single byte. Second, it enables **forward compatibility**: if a new version of the schema adds tag 6 for "pressure," old decoders that don't know about tag 6 can skip it (the wire type tells them how many bytes to skip). Fields can be added without breaking existing code. This is the core idea behind protobuf, Thrift, and Avro's field numbering systems.

### The Microser Wire Format

The demo format encodes each field as a **tag byte** followed by the value bytes. The tag byte packs the field number and wire type into a single byte:

```
tag = (field_number << 3) | wire_type
```

Four wire types cover all fixed-width integers:

| Wire Type | Size | Types |
|-----------|------|-------|
| 0 | 1 byte | uint8, int8 |
| 1 | 2 bytes | uint16, int16 |
| 2 | 4 bytes | uint32, int32 |
| 3 | 8 bytes | uint64, int64 |

Values are little-endian. Messages are framed with a 2-byte little-endian length prefix. That's the entire format — no varints, no length-delimited strings, no nested messages. This is deliberate: on an embedded system, you want fixed-width fields with predictable sizes.

### The IDL

The schema language blends Fory's clean message/enum syntax with Pascal's `case...of` variant records:

```
enum SensorType
    Temperature = 1
    Humidity = 2
    Combo = 3
end

message SensorReading
    uint8 sensor_id = 1
    uint32 timestamp = 2
    case SensorType kind = 3
        Temperature:
            int16 temperature = 4
        Humidity:
            uint16 humidity = 5
        Combo:
            int16 temperature = 4
            uint16 humidity = 5
    end
end
```

The `case` block is a discriminated union: the `kind` field (tag 3) determines which variant fields follow. The encoder writes only the variant's fields; the decoder skips anything it doesn't recognize. Variant fields that appear in multiple arms (like `temperature` in both `Temperature` and `Combo`) share the same tag number — tag 4 always means temperature regardless of variant.

### What the Generator Produces

Running `./gen.sh sensor.idl sensor` produces `sensor.h` and `sensor.c`. The header contains native C structs:

```c
struct sensor_reading {
    uint8_t sensor_id;
    uint32_t timestamp;
    sensor_type_t kind;
    int16_t temperature;
    uint16_t humidity;
};
```

Variant fields are flattened into the struct — the `kind` discriminant tells you which fields are valid. The generated encode function writes regular fields in order, then switches on the discriminant to write the appropriate variant fields. The decode function is a simple loop that reads tag bytes and dispatches by field number:

```c
while (pos < end) {
    uint8_t tag = buf[pos++];
    switch (tag >> 3) {
    case 1: pos = ms_read_u8(buf, pos, end, &msg->sensor_id); break;
    case 2: pos = ms_read_u32(buf, pos, end, &msg->timestamp); break;
    case 3: pos = ms_read_u8(buf, pos, end, &msg->kind); break;
    case 4: pos = ms_read_i16(buf, pos, end, &msg->temperature); break;
    case 5: pos = ms_read_u16(buf, pos, end, &msg->humidity); break;
    default: pos = ms_skip(buf, pos, end, tag & 7); break;
    }
    if (pos < 0) return -1;
}
```

The `default` case is the forward-compatibility mechanism: unknown tags are skipped using the wire type to determine how many bytes to advance. A decoder compiled against an older schema will silently skip fields it doesn't know about.

### On the Wire

A combo sensor reading (ID 7, timestamp 1712000000, temperature 23.4 C, humidity 65.5%) encodes to 17 bytes:

```
0f 00            payload length = 15
08 07            tag 1 (uint8 sensor_id) = 7
12 00 0c 0b 66   tag 2 (uint32 timestamp) = 1712000000
18 03            tag 3 (uint8 kind) = 3 (Combo)
21 ea 00         tag 4 (int16 temperature) = 234
29 8f 02         tag 5 (uint16 humidity) = 655
```

The same data in JSON — `{"sensor_id":7,"timestamp":1712000000,"kind":3,"temperature":234,"humidity":655}` — is 76 bytes. The tag-number approach is 4.5x more compact, and the decoder is a simple switch statement with no string matching, no memory allocation, and no dependencies beyond `<stdint.h>`.

The full demo source is in the `demo/` directory.

## Conclusion

The five-decade history of serialization formats reveals a recurring cycle. Each generation starts with a clean design solving real problems, then increasing complexity until a simpler alternative replaces it. XDR was simpler than ASN.1. SOAP was simpler than CORBA (briefly). REST was simpler than SOAP. Protobuf was simpler than ASN.1. Now gRPC has acquired enough features that some teams reach for simpler alternatives.

The enduring formats share common traits: a clear separation between schema and encoding, compact tagged or offset-based wire formats, field numbering for forward/backward compatibility, and multi-language code generation. XDR got the first two right in 1987. ASN.1 got schema/encoding separation right in 1984 — and is still unmatched for encoding flexibility. Protobuf found the sweet spot of simplicity and power in 2001. The zero-copy formats (Cap'n Proto, FlatBuffers, SBE) challenge the assumption that serialization requires an encode/decode step at all. Arrow and Parquet showed that flipping from row-oriented to columnar layout can matter more than any encoding trick.

Meanwhile, new approaches keep emerging. Fory's JIT compilation trades wire-format cleverness for runtime code generation. EXI tried to make XML competitive with binary formats — and technically succeeded, but the world moved on. OMG IDL outlived CORBA to find new life in DDS and robotics. And CSV, with no schema, no types, and no standard encoding, remains the most universally exchanged data format on earth.

The one universal lesson: never use a format that can execute code during deserialization. The graveyard of Java serialization vulnerabilities, .NET BinaryFormatter's removal, and Python pickle warnings should settle this permanently.

For most systems today, protobuf + gRPC is the default choice. But knowing what came before — and why — helps you recognize when the default isn't enough, and which alternative fits the constraints you actually face.
