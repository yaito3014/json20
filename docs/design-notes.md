# json20 設計ノート

設計判断・バグ修正の記録。各エントリは「選択肢 → 採用 → 却下理由」を残す。
Zenn 記事「設計判断の記録」の一次資料を兼ねる。

## 記録ルール

- 設計変更は判断ごとに 1 コミット。コミットメッセージにも「なぜ」「却下した代替案」を書く
- バグ修正はまず再現テストを書き、失敗を確認してから修正する
- コンパイラ別の挙動差・エラーは日付付きで記録する

## 検証環境（2026-09-15 時点）

- GCC 14.2.0 (Ubuntu 24.04, libstdc++ 14)
- Clang 18.1.3 (Ubuntu 24.04)
- CMake 3.28.3 / Catch2 v3.15.0 (FetchContent)

---

## 2026-09-15: libstdc++ 14 は `{:?}`（P2286 formatted escaping）未実装

`tests/main.cpp` のデバッグビジターが `std::println("{:?}", str)` を使っていたが、
GCC 14.2 の libstdc++ では `std::formatter<string_view>` が `?` 指定を受理せず
consteval の書式検査で硬いエラーになる:

```
error: call to consteval function 'std::basic_format_string<...>("on_null: {:?}")'
       is not a constant expression
/usr/include/c++/14/format:892: error: call to non-'constexpr' function
       'void std::__format::__failed_to_parse_format_spec()'
```

- 採用: `{}` に落とす（デバッグ出力なのでエスケープ表示は必須ではない）
- 却下: `__cpp_lib_format` での条件分岐 — テストの見通しが悪くなるわりに得るものが小さい
- 備考: GCC 13 はそもそも `<print>` 自体がない（libstdc++ に入ったのは GCC 14 から）。
  テストは `cxx_std_23` を要求しているため、実質 GCC 14+ / 対応 Clang が必要

## 2026-09-15: バグ修正 — `erase(key)` の条件が逆

症状（再現テストで確認、GCC 14.2）:

- `erase("foo")`（存在するキー）→ 何もしない（発見時に `return` していた）
- `erase("baz")`（存在しないキー）→ `lower_bound` の位置をそのまま `vec.erase(iter)` に渡すため、
  **隣のキーを誤削除**する（`{"bar", "foo"}` に対して `erase("baz")` は `"foo"` を消す）。
  キーが末尾より後ろに整列される場合は `erase(end())` で UB
- 既存テストは削除後に残存キーを読むだけで「消えたこと」を検証していなかったため発覚せず。
  再現テストでは `erase("baz")` 後のサイズ検査が誤削除のせいで「偶然」通る、という
  二重の見逃しも観測した（テストは失敗の仕方まで見ないと信用できない、の実例）

修正: 条件を正しい向き（未発見なら `return`、発見時に `erase`）に。あわせて

- 戻り値を `void` → `bool`（削除したか）に変更。`insert` / `emplace` が `bool` を返すのと対称。
  `std::map::erase(key)` の `size_type` は要素数が高々 1 なので `bool` で十分と判断
- 意味のない `template<class... Args>` ヘッダを除去（`emplace` からのコピー残骸と推測）

