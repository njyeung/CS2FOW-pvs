# Third-party code

`cgltf.h` and `cgltf.LICENSE` are from cgltf 1.15, commit `360db1a`.

`masked_occlusion_culling/` is Intel/Lund University's MaskedOcclusionCulling,
commit `6cbbd7621cce670cf081a44272669e240300879e`, under Apache-2.0. Its two
compiler-helper names are locally prefixed to avoid collisions with GCC 15 intrinsics;
non-MSVC builds disable strict-aliasing optimization as required by its SIMD implementation.

`vrf_licenses/` records the exact ValveResourceFormat 19.2 release files,
restored package graph, and redistribution notices copied into core packages.
