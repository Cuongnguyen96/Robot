#!/usr/bin/env bash
# Chạy trên máy thuê có 1x GPU >=80GB (H100/A100 80GB).
# Yêu cầu trước: `huggingface-cli login` với token đã được cấp quyền
# nvidia/Cosmos-Predict2.5-2B (xem README.md, Bước 0).
set -euo pipefail

git clone -b cosmos_predict_2.5_lora_clean https://github.com/terarachang/diffusers.git
cd diffusers/examples/cosmos

pip install -U "diffusers[torch]" transformers accelerate peft wandb

bash download_and_preprocess_datasets.sh

echo "Setup xong. Dataset mẫu (92 video pick-and-place) nằm ở gr1_dataset/train/."
echo "Muốn dùng data riêng: thay video/.txt trong gr1_dataset/train/ theo cấu trúc mô tả ở README.md."
