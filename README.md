# P-NIDS: Parallelized Network Intrusion Detection System 🛡️⚡

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![CUDA](https://img.shields.io/badge/CUDA-Enabled-green.svg)
![OpenMP](https://img.shields.io/badge/OpenMP-Multi--threading-orange.svg)
![MPI](https://img.shields.io/badge/MPI-Distributed-red.svg)
![ImGui](https://img.shields.io/badge/UI-Dear_ImGui-purple.svg)

P-NIDS is a high-throughput, trace-driven Network Intrusion Detection System built from scratch in C/C++. Designed as a semester project for Parallel and Distributed Computing (PDC), it leverages a hybrid parallel architecture (**MPI + OpenMP + CUDA**) to achieve massive hardware acceleration for real-time machine learning inference, bypassing the overhead of traditional Python frameworks.

## 🎯 Project Objective
Modern enterprise networks generate millions of packets per second. Detecting anomalies (like DDoS, Botnets, and Brute Force attacks) in real-time requires immense computational power. P-NIDS solves this by distributing the data ingestion, multi-threading the feature extraction, and utilizing GPU acceleration to run the ML classification math on thousands of network packets simultaneously.

## 🏗️ System Architecture

The system pipeline is designed for maximum throughput, mimicking a high-speed factory assembly line:

1. **The Orchestrator (MPI):** Uses a Master-Worker topology. The Master node reads the massive `CSE-CIC-IDS2018` network dataset and simulates real-time traffic by streaming data chunks to Worker nodes.
2. **The Preprocessor (OpenMP):** Inside each Worker, CPU cores use multi-threading (`#pragma omp parallel for`) to concurrently parse raw string data, normalize values, and extract numerical feature vectors.
3. **The Inference Engine (CUDA):** The preprocessed arrays are pushed to the GPU. A custom C++/CUDA kernel performs highly parallelized matrix multiplications using pre-loaded neural network weights, classifying packets as `0` (Benign) or `1` (Attack) in milliseconds.
4. **The Dashboard (Dear ImGui):** The Master node aggregates the GPU predictions and renders real-time traffic statistics and security alerts on a lightweight, low-overhead Windows GUI.

## 📊 Dataset & ML Model
* **Dataset:** We use a preprocessed, trace-driven CSV of the **CIC-IDS2017 / CSE-CIC-IDS2018** datasets.
* **Model:** A lightweight Multilayer Perceptron (MLP) Neural Network trained offline in Python/PyTorch. 
* **Integration:** The trained PyTorch weights are exported to a `model_weights.json` file, which the native C++ engine loads directly into GPU VRAM at startup.

