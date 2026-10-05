# Scripts — Task 2 (LoRA fine-tune Cosmos-Predict2.5-2B cho Video Robot Manipulation)

Hỗ trợ Task 2 trong `../../WFM.md`. Xem tài liệu đó để biết yêu cầu đầy đủ, tiêu chí
đánh giá, và giải thích kiến trúc. File này chỉ ghi chi tiết vận hành script.

## Yêu cầu phần cứng

- **Tối thiểu 1× GPU 80GB** (H100 hoặc A100 80GB). GPU 24GB trở xuống, hoặc Colab/Kaggle
  free tier, **không đủ VRAM để load checkpoint**. Nên thuê ở đâu (cloud bên thứ ba hay
  Google Cloud/AWS) — xem phần "Thuê GPU" đầu `../../WFM.md`.

## Bước 0 — Xin quyền tải checkpoint

`nvidia/Cosmos-Predict2.5-2B` trên HuggingFace là gated repo (NVIDIA Open Model License).
1. Đăng nhập HF, vào https://huggingface.co/nvidia/Cosmos-Predict2.5-2B, bấm "Agree and
   access repository".
2. Tạo access token (Settings → Access Tokens), rồi `huggingface-cli login` trên máy thuê.

## Bước 1 — Setup + tải dữ liệu mẫu

```bash
bash setup.sh
```

Clone repo (nhánh `cosmos_predict_2.5_lora_clean`), cài dependency, tải sẵn 92 video robot
pick-and-place — dataset [`nvidia/GR1-100`](https://huggingface.co/datasets/nvidia/GR1-100)
trên HuggingFace.

**Dùng dữ liệu riêng** (khuyến khích, xem mục điểm cộng trong `../../WFM.md`): copy `.mp4` +
`.txt` (prompt mô tả tương ứng) vào `gr1_dataset/train/videos/` và
`gr1_dataset/train/metas/`, cập nhật `metadata.csv` theo đúng cấu trúc đã có sẵn trong
bộ mẫu.

## Bước 2 — Train LoRA

```bash
bash train.sh
```

Mặc định: rank 32, bf16, 100 epoch, checkpoint mỗi 20 epoch, ~17 giờ trên 1×H100.
Theo dõi qua Weights & Biases (`wandb login` trước khi chạy).

## Bước 3 — Sinh video, so sánh trước/sau fine-tune

```bash
LORA_DIR=""                          bash infer.sh   # checkpoint gốc
LORA_DIR="./lora_out/checkpoint-100"  bash infer.sh   # checkpoint đã fine-tune
```

Ghép hai video cạnh nhau để so sánh (vd. `ffmpeg -i a.mp4 -i b.mp4 -filter_complex hstack out.mp4`).

## Ghi chú

- Script train/eval nằm ở fork
  [`terarachang/diffusers`](https://github.com/terarachang/diffusers/tree/cosmos_predict_2.5_lora_clean/examples/cosmos)
  (nhánh `cosmos_predict_2.5_lora_clean`, thư mục `examples/cosmos`), NVIDIA tự dẫn link
  trong bài hướng dẫn chính thức của họ — nhưng chưa merge vào `huggingface/diffusers`.
  Nên đọc qua code trước khi chạy trên GPU thuê tốn tiền.
- `cosmos-predict2` (repo pip package riêng) đã bị archive, khuyến nghị dùng
  `Cosmos-Predict2.5` qua `diffusers` như recipe này.
