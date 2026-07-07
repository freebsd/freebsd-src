#!/usr/local/bin/python3
# papagan-ggml-adapter.py
# Minimal adapter that executes an external LLM binary (e.g., llama.cpp bind)
# It provides a small CLI and HTTP-friendly stdin/out interface for integration.

import argparse
import os
import shlex
import subprocess
import sys

MODEL_PATH_DEFAULT = "/usr/local/share/papagan/models/ggml-model.bin"
LLM_BIN_DEFAULT = "/usr/local/bin/llama_cpp_bin"  # user must provide

def run_local(model_path, prompt, llm_bin=LLM_BIN_DEFAULT):
    if not os.path.isfile(model_path):
        print(f"Model not found: {model_path}", file=sys.stderr)
        return 1
    if not os.path.isfile(llm_bin):
        print(f"LLM binary not found: {llm_bin}", file=sys.stderr)
        return 2

    cmd = [llm_bin, "-m", model_path, "-p", prompt]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(p.stdout)
        return 0
    except subprocess.CalledProcessError as e:
        print(e.stderr, file=sys.stderr)
        return e.returncode

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Papagan ggml adapter stub")
    parser.add_argument("-m", "--model", default=os.environ.get("PAPAGAN_MODEL", MODEL_PATH_DEFAULT))
    parser.add_argument("-b", "--bin", default=os.environ.get("PAPAGAN_LLM_BIN", LLM_BIN_DEFAULT))
    parser.add_argument("-q", "--query", default=None)
    args = parser.parse_args()

    if args.query is None:
        prompt = sys.stdin.read().strip()
    else:
        prompt = args.query

    rc = run_local(args.model, prompt, args.bin)
    sys.exit(rc)
