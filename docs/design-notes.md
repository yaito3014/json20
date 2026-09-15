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
  → 後日却下（次項）

## 2026-09-15: 設計判断 — `cntrl` の述語パーサ化はしない（作者判断）

前項で「将来の判断」とした範囲述語パーサ（`c <= 0x1F` のような charT の数値比較）は
**導入しない**ことに決定。理由: C++ は実行文字エンコーディングを規定しておらず、
その中で文字を数値比較で分類する API は作りたくない、という作者の方針。

正確を期すための整理（記事でもこの順で書く）:

- 現行の `lit("\x00")`〜`lit("\x1F")` テーブルが述語版より「エンコーディング安全」な
  わけではない。16 進エスケープは文字→コード単位の変換を通らず**コード単位値を直接
  指定する**ので、`lit("\x10")` と `c == 0x10` は意味的に等価。0x00〜0x1F という数値への
  依存はどちらの形でも存在する
- したがってこの判断の実質は「依存を消す」ではなく**依存の置き場所の統制**:
  - エンコーディングへのコミットは `YK_JSON20_WIDEN_STRING` で書かれたリテラル表に
    閉じ込める。パーサコンビネータの語彙は「この正確な列にマッチする」（`lit`）だけで
    完結し、charT の数値領域に対する述語という一般化された API を持ち込まない
  - 述語パーサを公開すると、利用側が `std::iscntrl` 的な locale / エンコーディング依存の
    文字分類をコンビネータに書き出す入口になる。その拡張軸は封じておく
- 標準が保証するのは char8_t/char16_t/char32_t の UTF-8/16/32 のみ。`char` / `wchar_t` の
  エンコーディングは処理系定義であり、JSON（RFC 8259、Unicode 前提）を非 Unicode の
  実行エンコーディングで扱う場合の正しさは、リテラル表・述語のどちらでも保証できない。
  ここは「リテラル表がコミットの明示的な一覧になっている」ことを担保とする
- 受け入れるコスト: 32 連鎖 `alt` のコンパイル時間・可読性、および typo リスク。
  typo は再現テスト（0x00〜0x1F 全域ループ）で機械的に検出できるようになっており、
  これで足りるという判断

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

## 2026-09-15: 設計判断 — `try_as_string/array/object` と `try_at` は `T const*` を返す

旧: `try_as_array()` が `std::optional<std::vector<basic_json>>` を**値で**返していた。
kind 不一致の検査をしたいだけで部分木全体の深いコピーが発生する。`try_at` も同様に
`optional<basic_json>` の値返しで、要素 1 つの参照が欲しいだけでコピーになる。

検討した選択肢:

1. `std::optional<T>`（現状）— コピーコストが致命的。却下
2. `std::optional<std::reference_wrapper<T const>>` — コピーは消えるが、
   呼び出し側が `opt->get().size()` のように `get()` を挟む必要があり書き味が悪い。却下
3. C++26 `std::optional<T&>` — 意味的には理想だが、C++20 ライブラリなので使えない。見送り
4. **`T const*`（採用）** — `std::get_if` / `std::any_cast` のポインタ版という
   標準ライブラリに前例のある形。「失敗しうる・参照が欲しい」の標準的な回答であり、
   `if (auto* p = j.try_as_array())` のイディオムがそのまま書ける

住み分けの整理（この判断で API 全体の規則ができた）:

- **計算して返す値**（bool・数値: 字句から `from_chars` で毎回生成）→ `optional<T>` のまま
- **格納されている値**（string・array・object・要素）→ `T const*`

「格納物はポインタ、計算結果は optional」という一貫した規則になり、
どちらを返すかが実装の都合ではなく意味で決まる。テストでは
`try_as_array() == &as_array()` で「同じストレージを指す＝コピーなし」を固定した。

備考: 非 const 版（`T*` 返し）は現状 `as_*` 側にも存在しないため追加せず、
mutable アクセスの設計はまとめて別判断とする。

## 2026-09-15: 設計判断 — デフォルト構築は null（空 object をやめる）

旧: `json a;` は空 object だった。決め手は `operator[](std::size_t)` の自動拡張:
`vec.resize(idx + 1)` はデフォルト値で隙間を埋めるため、`a[3] = 1` とすると
`a[0..2]` が `{}`（空 object）になっていた。JSON の意味論では未設定要素は null であるべきで
（JavaScript の sparse array も、nlohmann::json の同じ操作も null で埋める）、
「デフォルト構築 = 空 object」は resize の埋め値という形で観測可能な嘘になっていた。

検討した選択肢:

1. **null にする（採用）** — JSON の意味論と一致。`resize` の埋め値がそのまま正しくなる。
   表現は kind = null + 字句 `"null"`（index 0 の文字列スロット）。パーサの
   `on_null` が作る表現と完全に同一なので、構築経路による差が生じない
2. 空 object を維持し `resize` の埋め値だけ null を明示 — 「デフォルトは object だが
   配列拡張の埋めは null」という二重基準になり、説明が一段複雑になる。却下
3. nlohmann 流: null のまま `operator[]` を許し object へ暗黙昇格 — 書き味は良いが、
   本ライブラリは「kind 不一致は常に throw」の厳格路線。ここだけ暗黙変換を入れると
   規則が「throw、ただし null は例外」になる。タイプミスした `j["typo"]["x"] = 1` が
   静かに構造を作ってしまう事故も含めて却下（`operator[]` は null に対しても throw）

随伴する変更:

- `json::object()` / `json::array()` の引数なし静的ファクトリを追加。
  「空 object が欲しい」の受け皿はデフォルト構築ではなくこちら
  （`json::object({})` でも同じものは作れるが、意図の表明として引数なし版を用意）
- `insert("foo", json{})` / `emplace("foo")` の結果は null になる
  （旧: 空 object）。「値を後で決める」プレースホルダの意味論としても null が自然
- `json{}` は空 init-list 扱いではなくデフォルト構築（C++ の規則どおり）なので null

破壊的変更: 旧コードの `json a; a.insert(...)` は `bad_json_access` を投げるようになる。
移行は `json a = json::object();`。

## 2026-09-15: コンパイラ挙動メモ — clang 18 + libstdc++ 14 は `variant<string, …>` を constexpr 構築できない

`static_assert(json{}.get_kind() == json_value_kind::null)` が GCC 14.2 では通るのに
clang 18.1.3（libstdc++ 14 ヘッダ使用）では
"placement new would change type of storage from `__aligned_membuf<basic_string>` to
`basic_string`" として非定数式扱いになる。json20 とは無関係の最小再現（10 行）:

```cpp
#include <variant>
#include <string>
#include <vector>
struct V { std::variant<std::string, std::vector<int>> v; constexpr V() : v(std::in_place_index<0>, "null") {} };
static_assert((V{}, true));  // GCC 14: OK / clang 18 + libstdc++ 14: エラー
```

- libstdc++ の `_Variadic_union` が使う placement new の書き方を、GCC は自前ヘッダとして
  受理するが clang のフロントエンドは定数式と認めない、という処理系間の相互運用問題
- ランタイム動作は clang でも全テスト相当の確認で問題なし。影響は
  「`basic_json` を含む値を定数式で作る」場合のみ
- 「constexpr パースの到達点」（メモの未解決問題）に直結する制約: `basic_json` の
  定数式構築は GCC でしか担保できていない。clang 19/20 や libc++ での再確認は TODO

## 2026-09-15: 設計判断 — `kind_` の別持ちは維持し、kind と storage の対を作れる場所を一元化

出発点は「理想的には variant のタグ型に enum を指定したいが、できないので `kind_` を
別で持っている」という問題意識。整理すると、`kind_` は variant タグの**複製ではない**:

- kind は 8 値（null / boolean / string / 数値 3 種 / array / object）、storage は 3 択
  （字句 `basic_string` / array / object）で、スカラー 6 種が index 0 を共有する
- つまり kind → index は**多対一の純関数**。字句保持設計（型を決めず文字列で持ち、
  アクセス時に変換する）の帰結であり、null を字句 `"null"` で持つ以上、
  内容から kind を再導出することもできない。`kind_` はどの設計でも消えない

検討した選択肢:

1. **8 択 variant（index == kind）** — `null_t{string}`, `boolean_t{string}`, … と
   ラッパー型を 6 つ作れば不変条件は構造的に消えるが、現在 `std::get<0>(data_)` 一発の
   字句アクセス（`as_*_unchecked`、deserialize、visitor）が全部 6 分岐 visit になる。
   静的に分かっていることに毎回 visit を払うことになり却下
2. **自前 tagged union（タグが本当に enum）** — 「理想」の直接実装だが、C++20 の
   constexpr union は `std::construct_at` 込みの機械が必要で、variant の例外安全も
   自前になる。clang + libstdc++ の constexpr variant 問題（前項）をこれで回避できる
   可能性はあるが、それは constexpr 到達点を攻める際の別判断として保留
3. **別持ちは維持し、対を作れる場所を一元化（採用）** — 真の問題は二重持ちではなく、
   旧 `private_construct` コンストラクタが `(kind, in_place_index)` を**別々に**受け、
   呼び出し側（visitor 含む）が誤った組を渡せたこと

実装:

- `detail::storage_index_of<K>`: kind → storage index の写像。コード上ここにしか存在しない
- private の `kind_construct_t<K>` タグ付きコンストラクタ / `make<K>(args...)` /
  `emplace_kind<K>(args...)`: kind をテンプレート引数で受け、index は写像から導出。
  **呼び出し側は kind しか指定できず、不一致は構文的に書けない**
- 公開コンストラクタは全て `kind_construct_t<K>` へ委譲、代入は `emplace_kind<K>`、
  visitor は `make<K>` を使用。`private_construct`（kind と index を自由に組めた唯一の穴、
  しかも public に置かれていた）は削除
- `emplace_kind` は storage を先に emplace してから `kind_ = K`。emplace が throw した
  場合に旧 kind が残る（variant が valueless になる可能性はあるが、kind の更新だけ
  先行して不一致を作るよりよい）

記事の観点: 「タグの複製に見えるものが実は semantic kind と storage representation の
分離だった。直すべきは複製ではなく、対を組む自由だった」。enum をタグにできる variant が
欲しいという要求は、`tagged_variant<Enum, Mapping>` として汎用化もできるが、
使用箇所が 1 つしかない段階での抽象化は見送った（YAGNI）。



## 2026-09-15: 設計判断 — EBCDIC の JSON もサポート対象とし、制御文字は UCN で綴る

方針決定: 本ライブラリは実行文字エンコーディングを ASCII 系と仮定せず、
**EBCDIC 実行環境の JSON も対象とする**。前提となる言語仕様の整理:

- 数値エスケープ `\xNN` は「コード単位値 NN をそのまま埋める」。**翻訳を通らない**
- UCN `\uNNNN` は「Unicode コード点 U+NNNN をそのリテラルのエンコーディングに
  **翻訳して**埋める」。表現不能なら ill-formed（静かな誤マッチではなくコンパイルエラー）。
  文字列リテラル内なら制御文字の UCN は C++11 から合法（ill-formed 規則はリテラル外のみ）
- 単純エスケープ `'\n'` は「処理系の改行文字」であり、JSON の `\n`（U+000A 固定）とは
  意味が違う。z/OS XL C では `'\n'` が NL (0x15 = U+0085) である伝統がある

実測（GCC 14.2、`-fexec-charset=IBM1047`）:

| リテラル | 既定 (UTF-8) | IBM1047 | 意味 |
|---|---|---|---|
| `'A'` | 0x41 | 0xC1 | 翻訳される |
| `"\u000A"` | 0x0A | 0x25 | 翻訳される（U+000A の EBCDIC 位置） |
| `"\x0A"` | 0x0A | 0x0A | 翻訳を通らない |
| `'\n'` | 0x0A | 0x25 | GCC は U+000A 扱いで翻訳（ただし規格保証なし） |

- GCC は単純エスケープも UCN と同値に翻訳したが、これは処理系の選択。
  規格に依拠できるのは UCN のみ
- clang 18 は `-fexec-charset=IBM1047` を受理しない（UTF-8 のみ）。
  この検証は GCC でしかできない（コンパイラ差の一次情報）

採用した綴りの規則（文法表のコーディング規約として固定）:

1. 印字可能な基本文字（`{` `"` 数字 `null` 等）: そのまま書く（コンパイラが翻訳）
2. JSON が意味づける制御文字（cntrl 表・ws・エスケープデコーダの出力）: **UCN のみ**。
   `\x`（翻訳を通らない）も単純エスケープ（意味が処理系依存）も使わない
3. `\x` は「生のコード単位を意図的に埋める」用途（テストでの生バイト注入）に限定

これにより文法表全体が「Unicode コード点を意味し、エンコーディングへの写像は
コンパイラが所有する」で統一された（従来は printable が翻訳あり・cntrl だけ生指定の混在）。

検証（tests/ebcdic.cpp、`-fexec-charset=IBM1047` が使える環境でのみ CMake が有効化。
全部 static_assert なのでコンパイル成功がテスト）:

- EBCDIC エンコードされた JSON が**定数式のまま**パースできる
- JSON の `\n` エスケープは 0x25（EBCDIC LF = U+000A）にデコードされる（0x15 ではなく）
- 文字列中の生バイト 0x25（この符号化では U+000A）は拒否される
- 生バイト 0x15（U+0085 NEL、JSON では非エスケープで合法）は受理される。
  この非対称は `c <= 0x1F` の数値述語では原理的に書けない — 述語パーサ却下（前項）が
  「好みの問題」ではなく EBCDIC サポートの必要条件だったことの裏付け

主張できる範囲と限界（正直に）:

- 保証するのはソースレベルの性質「ライブラリ自身は ASCII を仮定しない。リテラル翻訳は
  コンパイラに、数値変換は処理系の from_chars / to_chars に委譲する」まで
- `-fexec-charset` は TU 単位のシミュレーションであり、混成ツールチェーン
  （EBCDIC リテラル + ASCII ビルドの libc）では数値変換（from_chars の数字判定）が
  ずれる可能性がある。真の EBCDIC サポートは自己一貫した EBCDIC ツールチェーン
  （z/OS 等）上で成立する。パース（字句解析）だけなら from_chars 不要なので
  今回の検証範囲で完結している
- wchar_t 経路（`-fwide-exec-charset`）は未検証
