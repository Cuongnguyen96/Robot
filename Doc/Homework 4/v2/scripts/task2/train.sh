#!/usr/bin/env bash
# Chạy từ diffusers/examples/cosmos (sau khi setup.sh xong).
# Mặc định 100 epoch (khuyến nghị cho bài tập, ~17h trên 1xH100) thay vì
# 500 epoch dùng trong ví dụ gốc của NVIDIA — đổi lại nếu muốn train lâu hơn.
set -euo pipefail

export MODEL_NAME="nvidia/Cosmos-Predict2.5-2B"
export DATA_DIR="gr1_dataset/train"
export OUT_DIR="${OUT_DIR:-./lora_out}"
lora_rank=32

accelerate launch --mixed_precision="bf16" train_cosmos_predict25_lora.py \
  --pretrained_model_name_or_path=$MODEL_NAME \
  --revision diffusers/base/post-trained \
  --train_data_dir=$DATA_DIR \
  --train_batch_size=1 \
  --num_train_epochs=100 \
  --checkpointing_epochs=20 \
  --seed=0 \
  --output_dir=$OUT_DIR \
  --report_to=wandb \
  --height 432 --width 768 \
  --allow_tf32 --gradient_checkpointing \
  --lora_rank $lora_rank --lora_alpha $lora_rank
  # Thêm --use_dora ở dòng trên nếu muốn DoRA thay vì LoRA.

echo "Checkpoint LoRA nằm ở \$OUT_DIR/checkpoint-<epoch>/pytorch_lora_weights.safetensors"
