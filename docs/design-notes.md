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

## 2026-09-15: バグ修正 — serializer のバッファが未終端 + サイズ不足（引き継ぎメモ外の発見）

`try_as_*` の修正テストを書いたら `json(42u)` の `try_as_unsigned_integer<unsigned>() == 42u` が
**Catch2 の中でだけ**失敗し、単体の main では通る、という現象で発覚。原因は
`serializer<T, char>::serialize`:

```cpp
char buf[std::numeric_limits<T>::digits10 + 1];
auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), x);
...
return std::string(buf);   // ← ptr を使っていない
```

1. `std::string(buf)` は null 終端を仮定するが、`to_chars` は終端を書かない。
   `"42"` の直後のスタックゴミが数字だと字句が `"428…"` になり、読み出し時の
   `from_chars` が別の値を返す。スタックの内容次第なので、呼び出し文脈（Catch2 か
   単体 main か）で成否が変わる。UB が「たまたま通るテスト」を作る実例
2. `digits10 + 1` は不足。`digits10` は切り捨て（int64 で 18）なので、
   `INT64_MIN`（20 文字）や `UINT64_MAX`（20 文字）で `to_chars` が
   `value_too_large` → `runtime_error("to_chars error")` を投げる。
   `double` も `numeric_limits<double>::digits10 + 1 = 16` に対し最短表現は
   最大 24 文字（`-1.7976931348623157e+308`）

修正:

- サイズ: 整数は `digits10 + 2`（切り捨て分 + 符号）、浮動小数点は
  `max_digits10 + 8`（符号 + 小数点 + `e+XXXX`。80bit long double の `e+4932` も収まる）
- 構築: `std::string(buf, ptr)` で to_chars が書いた範囲だけを使う

字句保持設計（値を文字列で持つ）はシリアライズが唯一の正規化点なので、ここが壊れると
全アクセサが壊れる。roundtrip テスト（INT64_MIN / UINT64_MAX / double max / 0.1）と
字句そのものの検査（`as_string_unchecked() == "42"`）を追加した。

## 2026-09-15: バグ修正 — `try_as_*` がテンプレート実引数なしで `as_*_unchecked()` を呼ぶ

`try_as_unsigned_integer<T>` / `try_as_signed_integer<T>` / `try_as_floating_point<T>` の
内部が `as_unsigned_integer_unchecked()` のように `<T>` なしで委譲していた。
戻り値型からはテンプレート引数を推論できないため、インスタンス化した瞬間に
コンパイルエラー（GCC 14.2: "couldn't deduce template parameter 'T'"）。

- 既存テストが `try_as_*` を一度も呼んでいなかったため未発覚。
  「テンプレートはインスタンス化されるまでコンパイルされない」の実例
- 修正は `<T>` を明示するだけ。`try_as_*` 全種を通すテストを追加した
- この修正テストが serializer バグ（前項）を掘り当てた。テストの網を広げると
  隣のバグが引っかかる、という点も記事メモとして残す

## 2026-09-15: バグ修正 — `cntrl` の表で `\x00` が重複し `\x10` が欠落

制御文字（U+0000〜U+001F）を列挙する `alt(lit("\x00"), lit("\x01"), …)` の 32 行の中で、
17 番目にあるべき `"\x10"` が `"\x00"` になっていた（コピペ時の打ち間違いと推測）。
結果、文字列中の生の DLE (0x10) だけがすり抜けて受理されていた。

- 再現テストは 0x00〜0x1F を全部ループで拒否確認する形にした。
  32 個の手書き列挙は「1 個の typo が視認できない」ことが今回の原因なので、
  テスト側は列挙をループに置き換えて網羅を機械化
- 修正はデータ修正のみ。パーサ側の設計課題として「1 文字 alt の 32 連鎖」は
  コンパイル時間・可読性の両面で筋が悪く、範囲判定（`c <= 0x1F`）を行う
  述語パーサを導入する余地がある（今回はスコープ外、将来の判断として記録）

## 2026-09-15: バグ修正 + 設計判断 — 偽りの `noexcept` と try_as の失敗セマンティクス

変換系アクセサ `as_boolean_unchecked` / `as_*_integer_unchecked<T>` /
`as_floating_point_unchecked<T>` が `noexcept` だったが、内部の
`deserializer::deserialize` は `from_chars` 失敗時に `std::invalid_argument` を投げる。
kind が一致していてもオーバーフローは起きる（`json(300u)` を `uint8_t` で読む等）ので、
これは前提条件違反ではなくデータ依存の正常系エラー。実測: `json a(300u);`
`a.as_unsigned_integer<uint8_t>()` は catch 不能の `std::terminate`（exit 134）になった。

判断:

- **`unchecked` の意味を「kind 検査を省く」に限定**し、変換失敗は throw で報告する。
  よって変換系 4 つから `noexcept` を除去（`static_assert(!noexcept(...))` で固定）。
  却下案: 「unchecked = 全部 UB」路線。オーバーフローは呼び出し側が事前検査しづらく
  （字句を自前パースするしかない）、UB にする利益が薄い
- **`try_as_*` は「kind 不一致 or 変換失敗 → nullopt」** に統一し、`noexcept` を維持。
  実装は `try { return as_*_unchecked<T>(); } catch (...) { return nullopt; }`。
  C++20 の constexpr 関数は try ブロックを書けるが、定数評価中に throw に到達したら
  その時点で非定数式（= コンパイルエラー）になるだけで、従来と挙動差はない
- `try_as_boolean` は catch なしのまま。kind == boolean なら字句は必ず
  `"true"` / `"false"`（serializer とパーサの両方が保証）で、投げる経路が
  不変条件上存在しないため。ここに catch を書くと「投げうる」という誤った
  シグナルになる

参考: `std::optional` を返す以上、変換失敗の「理由」は潰れる。理由が要る用途は
throw 版 `as_*` を使う、という住み分け。

