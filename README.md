# GoldenSun

[![Build Status](https://gongminmin.visualstudio.com/GoldenSun/_apis/build/status/gongminmin.GoldenSun?branchName=main)](https://gongminmin.visualstudio.com/GoldenSun/_build/latest?definitionId=4&branchName=main)

GoldenSun is a GPU path tracer. It uses hardware ray tracing APIs to do the tracing. As an experimental project, there is no release plan, nor related to any product.

## Getting started
Before compiling GoldenSun, Python 3.x+ and CMake 3.18+ must be installed first. Then you can run Build.py to build the whole code base. Currently only VS2019 on Windows is supported. More compilers and OSs will be supported in the future.

## Principles
* Keep the algorithm simple
* No or less artificial parameter
* Easy to integrate into existing rendering engines

## Goals

### Primary Goals
* Full global illumination in path tracing
* Multi-bounce indirect lighting
* Indirect diffuse
* Ground truth mode with high spp
* Fast mode with low spp + denoiser

### Secondary Goals
* Realtime or interactive frame rate
* Indirect specular

## Contributing
As an open-source project, GoldenSun benefits greatly from both the volunteer work of helpful developers and good bug reports made by users. 

### Bug Reports & Feature Requests
If you've noticed a bug or have an idea that you'd like to see come real, why not work on it? Bug reports and feature requests are typically submitted to the [issue tracker](https://github.com/gongminmin/GoldenSun/issues).

## Why this name?
The name GoldenSun is inspired by the lyrics of the famous song Do-Re-Mi, Re/Ray: a drop of golden sun. Here we have a lot of rays in the process. So, yeah, golden sun.
