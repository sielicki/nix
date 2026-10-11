"""Shared provenance and CPU affinity helpers for the parser benchmark drivers."""

import hashlib
import os
from pathlib import Path
import sys


def file_sha256(path):
    # Bound the driver's memory use while hashing large files before launching
    # benchmark processes.
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def affinity_prefix(parser, cpu):
    if cpu is None:
        return []
    if not sys.platform.startswith("linux"):
        parser.error("--cpu requires Linux taskset")
    if cpu not in os.sched_getaffinity(0):
        parser.error(f"CPU {cpu} is not available to this process")
    return ["taskset", "-c", str(cpu)]


def record_library(parser, report, executable, directory=None):
    """Fingerprint a companion libexpr in the build or supplied directory.

    This lookup does not inspect the dynamic loader. The caller must ensure that
    the selected directory contains the matching libraries used by the executable.
    """
    candidates = [directory] if directory else [executable.parent.parent / "libexpr", executable.parent.parent / "lib"]
    for candidate in candidates:
        libraries = {p.resolve() for pattern in ("libnixexpr.so*", "libnixexpr*.dylib")
                     for p in Path(candidate).glob(pattern) if p.is_file()}
        # Saved builds may contain dereferenced copies of both the soname and
        # linker name. Accept identical bytes, but reject genuinely different
        # builds rather than recording a hash for the wrong library.
        if len(libraries) > 1 and len({file_sha256(p) for p in libraries}) > 1:
            parser.error(f"multiple libexpr versions in {candidate}; select a directory containing one build")
        if libraries:
            library = min(libraries)
            report["libexpr"] = str(library)
            report["libexpr_sha256"] = file_sha256(library)
            return
    if directory:
        parser.error(f"no libexpr shared library found in {directory}")
