# 本次完成的工作

## 工作範圍

完成 Sliding Window Space-Saving 的文獻對照與可手算正確性驗證，對齊團隊簡報的區塊視窗方向。本次未加入門檻告警或 Periodic-Reset，未修改 Space-Saving、CountSketch 或區塊視窗的核心演算法。

## 交付內容

1. [文獻對照](SLIDING_WINDOW_PAPER_COMPARISON_ZH.md)：整理 2005 基礎論文、2011 FSP／FSW 與團隊方法的關係，區分時間／封包數視窗、分區／直方圖機制、可引用的研究動機與不可直接搬用的保證。
2. [固定測試資料與說明](../examples/windowed_validation/README.md)：W=8、B=2、M=4，包含熱門來源切換及計數器替換兩組案例。
3. [驗證程式](../tests/windowed_walkthrough.cpp)：逐筆檢查查詢限制，在 6 個合法查詢點以原始資料重算頻率、核對精確基準、檢查候選及未見項目界限、來源過期及認證。
4. [查詢結果表](../results/validation/WALKTHROUGH_ZH.md) 與 [CSV](../results/validation/window_bounds.csv)：可由單一指令重建。
5. Makefile 將案例納入 `make test`，並提供 `make windowed-walkthrough` 產生結果。

## 驗證結論

- 第 8 筆填滿首個視窗，第 9–11 筆查詢被拒絕；第 12 筆只代表第 5–12 筆資料。
- shift 案例：熱門來源從 1 轉為 4；第 12 筆舊來源 1 不再保留於候選集合，第 16 筆來源 4 的界限為 [8,8]。
- replacement 案例：第 8 筆所有真實頻率為 1，回傳同頻來源可為有效答案，但不能認證唯一集合；第 12 筆改為來源 9，第 16 筆區間為 [8,8]。
- 兩組案例所有被檢查的頻率界限成立，取得認證的 Top-1 均與精確頻率一致。
- 本次驗證只支持這些案例的行為；一般界限推導與更廣泛測試仍見既有方法文件及測試，不由小樣本推論普遍效能。

## 重跑與檢查

```sh
make -C SpaceSaving_CountSketch_Integrated test windowed-walkthrough
```

新增 walkthrough 與既有四組測試全部通過，並確認 NDEBUG 編譯仍執行 walkthrough 檢查。固定輸入產生的結果可重現。原始 PDF 未複製或上傳。

## 後續由團隊確認

同環境 Linux 效能重跑不屬於本次驗證交付。

> 更新（2026-11-25）：Periodic-Reset 基準已於後續整合版（`4f48175`）加入；首頁已改為 Sliding Window 研究方向。此 walkthrough 已確認可與整合版一起編譯，`make test` 全部通過，重跑結果與原始檔案一致。
