#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd "${script_dir}/.." && pwd)"
retort_binary="${project_root}/build/retort"

if [[ ! -x "${retort_binary}" ]]; then
  echo "retort binary not found at ${retort_binary}" >&2
  exit 1
fi

output_path="${script_dir}/sample_index.sqlite"
"${retort_binary}" write --src_dir "${script_dir}/doc" --out "${output_path}"

exec "${retort_binary}" serve --index_path "${output_path}" --listen 127.0.0.1:9000
