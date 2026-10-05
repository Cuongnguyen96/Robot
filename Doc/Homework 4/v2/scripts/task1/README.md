# Scripts — Task 1 (Vision Transformer cho phân loại ảnh)

Hỗ trợ Task 1 trong `../../WFM.md`. Task này **không cần script cài đặt riêng** —
chạy trực tiếp trong một notebook Python (Google Colab, Kaggle Notebooks, hoặc máy cá
nhân có GPU). Dữ liệu (Imagenette) được tải tự động qua
`torchvision.datasets.Imagenette(size="320px", download=True)`, không cần clone repo
hay cài thêm gì ngoài `torch`/`torchvision`.

Xem `WFM.md` (Task 1):

* Mục 2 — yêu cầu kỹ thuật đầy đủ (tokenization, kiến trúc ViT-Tiny_16, siêu tham số).
* Mục 6 — pseudocode gợi ý cấu trúc code (học viên tự viết lại bằng `torch.nn`, cấm
  dùng checkpoint pretrained dưới mọi hình thức — xem "Liêm chính học thuật" ở Mục 2).
* Mục 7 — code mẫu vẽ đồ thị loss/accuracy và (tuỳ chọn) attention map của token `[CLS]`.
