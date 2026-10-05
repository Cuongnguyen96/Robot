#!/usr/bin/env bash
# Chạy từ diffusers/examples/cosmos, sau khi train.sh đã tạo checkpoint.
#
# So sánh trước/sau fine-tune (Mục 7.2 của WFM.md):
#   LORA_DIR=""                          bash infer.sh   # checkpoint gốc, không adapter
#   LORA_DIR="./lora_out/checkpoint-100"  bash infer.sh   # checkpoint đã fine-tune
set -euo pipefail

export DATA_DIR="gr1_dataset/test"
export OUT_DIR_EVAL="${OUT_DIR_EVAL:-./eval_out}"
LORA_DIR="${LORA_DIR:-}"

ARGS=(
  --data_dir "$DATA_DIR"
  --output_dir "$OUT_DIR_EVAL"
  --height 432 --width 768
  --num_output_frames 93
  --num_steps 36
  --seed 0
)

if [[ -n "$LORA_DIR" ]]; then
  ARGS+=(--lora_dir "$LORA_DIR")
  echo "Sinh video với adapter LoRA: $LORA_DIR"
else
  echo "Sinh video với checkpoint gốc (không adapter) — dùng để so sánh."
fi

python eval_cosmos_predict25_lora.py "${ARGS[@]}"

echo "Video sinh ra nằm ở $OUT_DIR_EVAL"
