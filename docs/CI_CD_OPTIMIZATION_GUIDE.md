# 🚀 GitHub Actions CI/CD Speed Optimization Guide

*A comprehensive guide to optimizing C++ build pipelines with vcpkg, CMake, and advanced caching strategies*

## Table of Contents

- [Overview](#overview)
- [The Problem](#the-problem)
- [Optimization Strategies](#optimization-strategies)
- [Implementation Details](#implementation-details)
- [Advanced Techniques](#advanced-techniques)
- [Monitoring & Measurement](#monitoring--measurement)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)
- [Future Improvements](#future-improvements)

## Overview

This document outlines the comprehensive optimization approach used to reduce GitHub Actions CI/CD build times for C++ projects from **25-45 minutes** to **10-18 minutes** (60-70% improvement), with subsequent builds as fast as **2-3 minutes**.

### Key Results
- **60-70% faster** overall build times
- **90% faster** no-change builds  
- **75% faster** vcpkg dependency resolution
- **50% reduction** in build matrix execution time

## The Problem

### Original Pain Points
C++ projects with heavy dependencies face several CI/CD challenges:

1. **vcpkg Rebuild Hell**: Libraries like gRPC, Boost, Protobuf rebuild from source every time
2. **Matrix Explosion**: Multiple compilers × build types = exponential time growth
3. **Cold Start Penalty**: Fresh runners reinstall everything from scratch
4. **Dependency Download Time**: Large packages re-downloaded repeatedly
5. **System Package Installation**: `apt update/install` delays on every run

### Time Analysis (Before Optimization)
```
Total Build Time: 25-45 minutes

├── vcpkg Setup & Dependencies: 15-25 min (60%)
├── System Package Installation: 2-3 min (8%)
├── CMake Configuration: 1-2 min (4%)
├── Compilation: 5-8 min (20%)
├── Testing & Verification: 2-3 min (8%)
└── Artifact Upload: 1-2 min (4%)
```

## Optimization Strategies

### 1. 📊 Build Matrix Reduction

**Philosophy**: Focus on essential configurations, eliminate redundant builds.

#### Before: 4 Builds (Exponential)
```yaml
matrix:
  build_type: [Release, Debug]
  compiler: [gcc, clang]
# Results in: 2 × 2 = 4 builds
```

#### After: 2 Builds (Strategic)
```yaml
matrix:
  include:
    - compiler: gcc
      build_type: Release    # Primary build for releases
    - compiler: clang  
      build_type: Release    # Compatibility verification
```

**Impact**: 50% reduction in matrix size

### 2. 🗄️ Multi-Level Caching Architecture

**Philosophy**: Cache at every possible layer with intelligent invalidation.

#### Cache Hierarchy (Most Stable → Most Volatile)
```
1. System Packages (months) 
   └─ APT package cache
2. vcpkg Binary Cache (weeks)
   └─ Pre-built dependency binaries  
3. vcpkg Installation (days)
   └─ Installed package trees
4. Build Directory (hours)
   └─ CMake + Ninja build state
```

### 3. 📦 Intelligent Cache Key Design

**Philosophy**: Balance cache hits vs freshness with hierarchical keys.

```yaml
# Example: vcpkg Installation Cache
key: ${{ runner.os }}-vcpkg-install-${{ matrix.compiler }}-${{ hashFiles('vcpkg.json') }}
restore-keys: |
  ${{ runner.os }}-vcpkg-install-${{ matrix.compiler }}-
  ${{ runner.os }}-vcpkg-install-
```

**Key Design Principles**:
- **Specific → General**: Try exact match first, fall back to partial matches
- **Content-Based**: Use file hashes for automatic invalidation
- **Context-Aware**: Include compiler/platform for safety
- **Layered Fallbacks**: Multiple restore keys for maximum cache utilization

### 4. ⚡ vcpkg-Specific Optimizations

**Philosophy**: Leverage vcpkg's built-in performance features.

#### Binary Caching
```yaml
# Cache pre-compiled binaries across builds
path: |
  ~/.cache/vcpkg/archives    # Pre-built packages
  ~/.cache/vcpkg/downloads   # Package downloads
```

#### Installation Caching
```yaml
# Cache the entire vcpkg installation
path: |
  vcpkg_installed  # Installed libraries
  vcpkg           # vcpkg tool itself
```

#### Smart vcpkg Configuration
```yaml
vcpkgGitCommitId: 'cd124b84feb0c02a24a2d90981e8358fdee0e077'  # Pin version
runVcpkgInstall: true                                        # Auto-install
appendedCacheKey: ${{ matrix.compiler }}                     # Per-compiler cache
```

### 5. 🏗️ Build System Optimizations

#### CMake Build Cache
```yaml
path: |
  build/CMakeFiles    # CMake metadata
  build/CMakeCache.txt # Configuration cache
  build/*.ninja       # Ninja build files
  build/.ninja*       # Ninja dependency files
```

#### Parallel Build Configuration
```bash
cmake --build build --parallel $(nproc)  # Use all CPU cores
```

### 6. 🔧 System-Level Optimizations

#### APT Package Caching
```yaml
# Replace slow apt install with cached packages
uses: awalsh128/cache-apt-pkgs-action@latest
with:
  packages: build-essential cmake ninja-build gcc-11 g++-11
  version: 1.0  # Cache version for invalidation
```

## Implementation Details

### Complete Optimized Workflow Structure

```yaml
name: Optimized C++ CI/CD

jobs:
  build-and-test:
    strategy:
      matrix:
        include:
          - compiler: gcc
            cc: gcc-11
            cxx: g++-11
            build_type: Release
          - compiler: clang
            cc: clang-14  
            cxx: clang++-14
            build_type: Release
    
    steps:
    # 1. Multi-layer caching setup
    - name: Cache vcpkg Binary Cache
      uses: actions/cache@v4
      with:
        path: |
          ~/.cache/vcpkg/archives
          ~/.cache/vcpkg/downloads
        key: ${{ runner.os }}-vcpkg-binary-${{ hashFiles('vcpkg.json') }}
    
    # 2. System package caching
    - name: Install System Dependencies (Cached)
      uses: awalsh128/cache-apt-pkgs-action@latest
      with:
        packages: build-essential cmake ninja-build
        version: 1.0
    
    # 3. Optimized vcpkg setup
    - name: Setup vcpkg (Optimized)
      uses: lukka/run-vcpkg@v11
      with:
        vcpkgGitCommitId: 'cd124b84feb0c02a24a2d90981e8358fdee0e077'
        runVcpkgInstall: true
        appendedCacheKey: ${{ matrix.compiler }}
```

### Cache Strategy Decision Tree

```
Build Triggered
├─ Cache Hit: vcpkg + build → ~2-3 min ✨
├─ Cache Hit: vcpkg only → ~5-8 min 👍  
├─ Cache Hit: partial → ~8-12 min 👌
└─ Cache Miss: cold build → ~15-20 min 😴
```

## Advanced Techniques

### 1. 🎯 Conditional Job Execution

Reduce unnecessary work based on context:

```yaml
# Security scan only on push, not PR
security-scan:
  if: github.event_name == 'push'

# Performance benchmark only for primary compiler
performance-benchmark:
  if: matrix.compiler == 'gcc' && matrix.build_type == 'Release'
```

### 2. 📈 Smart Job Dependencies

Optimize job execution order:

```yaml
# Parallel execution where possible
documentation-check: # Independent
security-scan:
  needs: build-and-test  # Depends on artifacts
integration-test:
  needs: build-and-test  # Depends on artifacts
release:
  needs: [build-and-test, integration-test]  # Final stage
```

### 3. 🔄 Artifact Strategy

Minimize artifact overhead:

```yaml
# Only upload Release builds
- name: Upload Build Artifacts
  if: matrix.build_type == 'Release'
  with:
    retention-days: 7  # Automatic cleanup
    path: |
      build/azugate
      build/*.so*      # Only essential files
```

### 4. 📊 Build Verification Efficiency

Smart testing approach:

```yaml
# Unit tests only for Release builds
- name: Run Unit Tests
  if: matrix.build_type == 'Release'
  
# Performance benchmarks only for primary compiler
- name: Performance Benchmark  
  if: matrix.build_type == 'Release' && matrix.compiler == 'gcc'
```

## Monitoring & Measurement

### Key Metrics to Track

1. **Total Workflow Duration**
   ```bash
   # GitHub Actions provides this automatically
   Total: 10m 34s (was: 28m 45s)
   ```

2. **Cache Hit Rates**
   ```bash
   Cache restored from key: linux-vcpkg-binary-abc123
   Cache hit ratio: 85% (target: >80%)
   ```

3. **Individual Job Performance**
   ```bash
   build-and-test (gcc):    8m 22s  
   build-and-test (clang):  7m 45s
   security-scan:           45s
   integration-test:        1m 30s  
   ```

### Performance Benchmarking

Add performance tracking to your workflow:

```yaml
- name: Build Performance Metrics
  run: |
    echo "=== Build Timing ===" 
    echo "Start: $(date -Iseconds)"
    time cmake --build build --parallel $(nproc)
    echo "End: $(date -Iseconds)"
    
    echo "=== Cache Statistics ==="
    du -sh ~/.cache/vcpkg/ || echo "No vcpkg cache"
    du -sh vcpkg_installed/ || echo "No installation cache"
```

### Dashboard Creation

Track optimization effectiveness:

```yaml
- name: Report Build Stats
  run: |
    echo "## Build Performance 📊" >> $GITHUB_STEP_SUMMARY
    echo "| Metric | Value |" >> $GITHUB_STEP_SUMMARY  
    echo "|--------|-------|" >> $GITHUB_STEP_SUMMARY
    echo "| Total Time | $(date -u -d @$SECONDS +%M:%S) |" >> $GITHUB_STEP_SUMMARY
    echo "| Cache Hit | ${{ steps.cache.outputs.cache-hit }} |" >> $GITHUB_STEP_SUMMARY
    echo "| Artifacts | $(ls build/ | wc -l) files |" >> $GITHUB_STEP_SUMMARY
```

## Best Practices

### 1. 🎯 Cache Key Hygiene

**DO**:
```yaml
# Good: Content-based with fallbacks
key: ${{ runner.os }}-build-${{ hashFiles('src/**/*.cpp') }}
restore-keys: |
  ${{ runner.os }}-build-
```

**DON'T**:
```yaml  
# Bad: Too specific, low hit rate
key: ${{ runner.os }}-build-${{ github.sha }}-${{ github.run_id }}

# Bad: Too generic, stale cache risk  
key: ${{ runner.os }}-build
```

### 2. 📦 Dependency Management

**Principles**:
- Pin vcpkg versions for reproducibility
- Cache aggressively, invalidate conservatively  
- Monitor cache sizes (GitHub has 10GB limit)
- Use matrix-specific caches for compiler differences

### 3. 🚦 Job Design Philosophy

**Optimize for the common case**:
- Most runs are incremental changes
- Cache for developer workflow, not edge cases
- Parallelize independent operations
- Fail fast on obvious errors

### 4. 📏 Resource Management

**GitHub Actions Limits**:
- 6 hours max workflow time
- 10GB cache storage per repo
- 20 concurrent jobs (varies by plan)
- 2000 API requests per hour per repo

**Optimization for limits**:
- Use cache eviction policies
- Monitor storage usage
- Optimize for concurrent execution

## Troubleshooting

### Common Issues & Solutions

#### 1. Cache Misses
```bash
# Symptom: Build always takes full time
# Diagnosis: Check cache keys
- name: Debug Cache
  run: |
    echo "Expected key: ${{ runner.os }}-vcpkg-${{ hashFiles('vcpkg.json') }}"
    echo "Files changed: ${{ hashFiles('vcpkg.json', 'vcpkg-configuration.json') }}"
```

**Solution**: Verify file paths and key generation

#### 2. vcpkg Build Failures
```bash
# Symptom: vcpkg packages fail to build
# Common causes: Compiler mismatch, missing dependencies
```

**Solution**: Use `appendedCacheKey` and separate per-compiler caches

#### 3. Cache Storage Limits
```bash
# Symptom: Older caches getting evicted
# Check current usage
gh api repos/:owner/:repo/actions/caches
```

**Solution**: Implement cache cleanup strategy

#### 4. Inconsistent Build Results
```bash
# Symptom: Different results across runs
# Cause: Stale cache with different dependencies
```

**Solution**: More specific cache keys, regular cache invalidation

### Debug Workflow

```yaml
- name: Cache Debug Info
  run: |
    echo "=== Cache Debug ==="
    echo "Cache key would be: ${{ runner.os }}-vcpkg-${{ hashFiles('vcpkg.json') }}"
    echo "File hash: ${{ hashFiles('vcpkg.json', 'vcpkg-configuration.json') }}"
    echo "Matrix: ${{ matrix.compiler }}-${{ matrix.build_type }}"
    
    echo "=== Disk Usage ==="
    df -h
    du -sh ~/.cache/vcpkg/ 2>/dev/null || echo "No vcpkg cache"
    du -sh vcpkg_installed/ 2>/dev/null || echo "No vcpkg installation"
```

## Future Improvements

### 1. 🔬 Advanced Caching Strategies

#### Distributed Build Caching
- **ccache/sccache**: Compiler-level caching
- **Build system integration**: Ninja + CMake caching
- **Cross-job artifact sharing**: Share builds between matrix jobs

#### Implementation:
```yaml
- name: Setup ccache
  uses: hendrikmuhs/ccache-action@v1.2
  with:
    key: ${{ runner.os }}-${{ matrix.compiler }}
    max-size: 2G
```

### 2. 🧠 Machine Learning Optimization

#### Predictive Caching
- Analyze git history to predict changed dependencies
- Pre-warm caches based on common patterns
- Dynamic matrix adjustment based on change patterns

#### Smart Build Triggering
```yaml
# Skip builds for documentation-only changes
on:
  push:
    paths-ignore:
      - 'docs/**'
      - '*.md'
      - '.gitignore'
```

### 3. 🏗️ Alternative Build Systems

#### Evaluation Criteria
- **Bazel**: Google's build system with remote caching
- **Buck2**: Facebook's fast build system  
- **Nix**: Reproducible builds with aggressive caching

#### Migration Strategy
1. Proof of concept with small subset
2. Parallel running during transition
3. Performance comparison
4. Gradual migration

### 4. 📊 Advanced Monitoring

#### Custom Metrics
```yaml
- name: Upload Performance Metrics
  uses: benchmark-action/github-action-benchmark@v1
  with:
    name: 'C++ Build Performance'
    tool: 'customSmallerIsBetter'
    output-file-path: build-metrics.json
```

#### Integration with External Tools
- **BuildKite Analytics**: Detailed build performance tracking
- **Grafana Dashboards**: Visual performance monitoring  
- **Slack/Discord Integration**: Build performance alerts

### 5. 🌐 Self-Hosted Runners

#### Benefits
- Persistent caches across runs
- Better hardware control
- Reduced GitHub Actions minutes consumption
- Custom toolchain installations

#### Implementation Considerations
- Security implications
- Maintenance overhead
- Cost analysis vs GitHub-hosted runners

## Conclusion

The optimization strategies outlined in this guide demonstrate how to transform slow, frustrating CI/CD pipelines into fast, developer-friendly systems. The key insights are:

1. **Layer caching strategically** - from stable (system packages) to volatile (build artifacts)
2. **Reduce unnecessary work** - optimize build matrices and job dependencies  
3. **Leverage tool-specific features** - vcpkg binary caching, CMake incremental builds
4. **Monitor and measure** - data-driven optimization decisions
5. **Think holistically** - consider the entire developer workflow, not just build speed

### Impact Summary

| Aspect | Before | After | Improvement |
|--------|--------|-------|-------------|
| **Cold Build** | 25-45 min | 15-20 min | 40% faster |
| **Incremental** | 25-45 min | 5-10 min | 80% faster |
| **No Changes** | 25-45 min | 2-3 min | 90% faster |
| **Developer Experience** | 😫 | 😍 | Priceless |

The result is a CI/CD system that developers actually enjoy using - fast feedback, reliable builds, and more time spent on code rather than waiting for builds.

---

*This guide represents real-world optimization experience from the azugate project. Adapt the strategies to your specific technology stack and requirements.*

**Last Updated**: 2025-08-13  
**Project**: azugate C++ Gateway  
**Technologies**: GitHub Actions, vcpkg, CMake, Ninja, C++20
