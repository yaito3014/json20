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
  → **2026-10-15 訂正: 誤り。パーサ本体と `checked_string` も影響を受ける（後述の訂正エントリ）**
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

## 2026-10-15: バグ修正 — 数値文法が RFC 8259 に違反（`1.` / `1e` / `1e+` を受理）

RFC 8259 の数値文法は `frac = decimal-point 1*DIGIT`、`exp = e [ minus / plus ] 1*DIGIT` だが、
実装は小数部・指数部の数字列を `many(parse0to9)`（**0 回以上**）で書いていたため、
数字のない `1.` / `1.e5` / `1e` / `1e+` / `1e-` / `1.5e` を受理していた。
配列やオブジェクトの中（`[1.]`、`{"a":1e}`）でも同じ。

影響が大きい理由: `checked_string` の存在意義は「不正な JSON リテラルをコンパイル時に弾く」ことなので、
この違反は不正な JSON にコンパイル時のお墨付きを与えていたことになる。
さらに、受理された `"1."` は浮動小数点 kind として格納され、後で `from_chars` に渡される。
不正な字句がライブラリ内部へ入り込んでいた。

再現テスト（tests/parse.cpp）は `checked_string` と同じ経路（no-op ビジターでの定数評価）を
`static_assert` で検査する形にした。上の 8 ケースすべてで失敗することを確認してから修正した。
既に正しく拒否できていた入力（`-` / `.5` / `01` / `+1`）と、受理すべき境界ケース
（`-0`、`1e-05`、`-0.0e-0` など）も同じテストで固定している。

修正: `parse_digits = seq(parse0to9, many(parse0to9))`（1 回以上）を局所的に定義し、
小数部と指数部の両方で使う。整数部の既存の書き方 `seq(parse1to9, many(parse0to9))` と同じ形。

- 却下: 汎用の `many1` / `some` コンビネータを追加する案。今のところ使用箇所は 2 つだけで、
  どちらも既存の `seq` + `many` で書ける。コンビネータの語彙は必要になってから増やす
  （述語パーサ却下と同じく、語彙を小さく保つ方針）
- ついでの修正: 数値パーサの失敗時の戻り値が `{nullopt, res2.rest}`（`-` を消費した後の位置）だった。
  他のパーサはすべて失敗時に入力位置を変えない `{nullopt, str}` を返す。呼び出し側の `alt` は
  失敗時の `rest` を見ないため、現状では観測できない。それでも、コンビネータ間の約束
  （失敗したら何も消費しない）に揃えた

## 2026-10-15: 訂正 — clang 18 + libstdc++ 14 では `checked_string` 自体が機能しない（main 時点から）

2026-09-15 のノートで「clang の constexpr variant 問題の影響は `basic_json` を定数式で作る場合のみ」と
書いたのは誤りだった。数値文法修正の再現テストを clang 18 でもコンパイルして発覚した。

正しい条件（json20 に依存しない最小再現で確認）:

```cpp
using T = std::tuple<std::string_view, std::vector<std::string_view>>;
constexpr bool f() {
  T t{"1", {}};
  std::variant<std::string_view, T> v = t;
  return v.index() == 1;
}
static_assert(f());  // GCC 14: OK / clang 18 + libstdc++ 14: 非定数式
```

- libstdc++ の `variant` は、**トリビアルに破棄できない選択肢**（`vector` を含む `tuple` など）を
  `__aligned_membuf` 上に placement new で構築する。clang はこれを定数式と認めない
  （"placement new would change type of storage"）。`string` も `vector` もトリビアルに
  破棄できないので、前回の `variant<string, …>` も同じ規則で説明がつく
- パーサコンビネータの `alt` は `variant<T, U>` を返す。整数部
  `alt(parse0, seq(parse1to9, many(parse0to9)))` の第 2 選択肢は
  `tuple<string_view, vector<string_view>>` なので、`1`〜`9` で始まる数値や配列を
  パースした時点で定数評価が止まる
- 実測: **元の main の時点で**、clang 18 では `checked_string` に `"1234"`、`"12.34"`、`"[1,2]"` を
  渡しても定数評価できない。今回の変更による退行ではない。`tests/parse.cpp` は
  既存の `checked_string` テストの時点で clang ではコンパイルできなかった。
  CMake が GCC でしかテストしていなかったので気づかなかった
- GCC 14 ではすべて通る。「コンパイル時 JSON パース」という README の謳い文句は
  現状 GCC でしか成立していない

対処方針（未決定・選択肢だけ記録）:

1. コンビネータの戻り値から非トリビアル型を除く。`many` が `vector` を作らないようにする
   （マッチ範囲の `string_view` だけ返す）。パース結果の値の大半はどうせ捨てているので、
   自然な方向。`sep_by` は既に範囲だけを返している
2. libc++ では起きない可能性がある（libc++ は未導入のため未検証）。clang + libc++ の組み合わせを
   CI に入れて確認する
3. clang 19 以降で直っているかを確認する

いずれにせよ、CI に clang を入れないと再発に気づけない。「次にやること」の CI の優先度を上げる根拠になる。

## 2026-10-15: バグ修正 — 閉じていない文字列でプロセスが落ちる（`except` が空入力で `substr(1)`）

コンビネータの設計見直しのために `except` を読んでいて発見した。`except(p)` は「`p` がマッチしなければ 1 文字消費する」コンビネータだが、
入力が空のときも `{str.substr(0, 1), str.substr(1)}` を返していた。空の `string_view` に対する `substr(1)` は
`std::out_of_range` を投げる。

- 実行時: `json_parser::try_parse("\"abc")` が **`std::terminate`（exit 134）** になる。
  パーサ関数はすべて `noexcept` なので、例外は外に出られない。「不正な入力は `nullopt` を返す」はずの
  `try_parse` が、末尾で切れた入力（閉じていない文字列、`["abc`、`{"a` など）を渡しただけでプロセスを落としていた。
  信頼できない入力を受ける API としては最悪の失敗の仕方。元の main でも同じ
- コンパイル時: `checked_string` では throw に到達して非定数式になり、結果的にはコンパイルエラーで「拒否」されていた。
  ただし理由は `__throw_out_of_range_fmt` で、偶然正しい結果になっていただけ
- 予想していたのは「空のマッチで成功し続けて `many` が無限ループする」だったが、実際は `substr(1)` の
  範囲外例外が先に起きた。どちらにしても、「空入力に対して成功を返す」ことが根本原因

修正: 入力が空なら失敗（`{nullopt, str}`）を返す。1 文字も消費できないので、これが正しい意味。
再現テストは `static_assert`（定数評価）と実行時の `try_parse` / `parse` の両方で、切れた入力 5 種を検査する。

コンビネータ設計への教訓（次項の設計変更に引き継ぐ）: 「成功したなら必ず 1 文字以上消費する」か、
「`many` は前進しなかったら止まる」のどちらかを、すべてのコンビネータで守る必要がある。
今は `opt` が空のマッチで成功するので、`many(opt(...))` と書けば無限ループする。

## 2026-10-15: 設計判断 — コンビネータを「認識だけを行う中核 + 意味の処理は明示的なアクション」に（方針）

clang でパーサが定数評価できない原因は、コンビネータが `vector` などの所有型を値として組み立てていることだった。
対策として「コンビネータはマッチした範囲（入力の `string_view`）だけを返す」設計を検討した。
作者の懸念は 2 点: ダングリングを起こさないことと、汎用パーサとしての設計に影響することだった。

ダングリングについての結論: 新しい危険は生まれない。条件は次の規則 1 つ。

- **コンビネータは自分の入力を切り出すだけで、自前のバッファを作ってそれを切り出すことはしない。**
  これを守れば、すべての範囲は利用者が渡したバッファを指す
- 公開 API の戻り値は `basic_json` / `bool` で、ビューは外に出ない。外に出る唯一の経路はビジターの
  コールバックで、これは既存の `on_number_*` / `_raw` と同じ条件（コールバック中だけ有効）
- `checked_string` は `consteval` なので、一時オブジェクトを指すビューは定数式になれず、コンパイルエラーになる
- 例外は文字列のデコード結果。`"a\nb"` のデコード結果は入力に存在しないので、範囲では表せない。
  文法の認識（範囲）とデコード（所有）を 2 段階に分けて閉じ込める

汎用パーサとしての設計: 型付きの結果（`alt` が `variant`、`seq` が `tuple`、`many` が `vector`）を
実際に使っていたのは文字列のデコード 1 か所だけで、意味のある出力はすべてビジターのコールバックで
外に出していた。つまり json20 は実質的に、すでに「認識はコンビネータ、意味の処理はコールバック」で動いていた。

選択肢:

- A. 認識だけを行う設計（全コンビネータが範囲を返す）。コンビネータを内部実装と割り切るなら最適
- **B. 認識だけを行う中核 + 意味の処理を明示する `act(p, f)`（採用）**。コンビネータを将来公開する可能性を
  作者が残したいため。汎用性は「結果の型」ではなく「副作用のコールバック」で確保する。
  ビジター設計と同じ考え方で一貫する。`act` は必要になった時点で追加する
- C. 型付きの結果を残し、所有型だけを避ける。コンビネータごとに方針がばらつくので却下

実装は 3 コミットに分ける: (1) `lit` がリテラルを値で持つ、(2) 全コンビネータが範囲を返す、
(3) 文字列の認識とデコードを分離する。

計測（変更前、GCC 14.2、-O0、5 回の中央値。計測用 TU とスクリプトは `bench/compile_time/`）:

| TU | 変更前 |
|---|---|
| 全 charT でパーサ + DOM ビジターを実体化 | 17.4 秒 |
| `checked_string` 5 本の定数評価 | 3.6 秒 |

### (1) `lit` がリテラルを値で持つ

調査で見つけた潜在的なダングリングの形: `lit_parser` はリテラルを `string_view` で持っていた。
リテラルの多くはローカル変数へのコピー（`auto const space = YK_JSON20_WIDEN_STRING(...)` など 20 か所以上）
なので、パーサオブジェクトがローカル変数を指すビューになっていた。今は同じ関数の中で使い切っているので
問題は起きていない。しかし、コンビネータを公開して「パーサを作って返す関数」を書けるようにした瞬間に
ダングリングする。

修正: `lit_parser<N>` が `basic_fixed_string<charT, N>` を値で持つ（数バイトなのでコピーの負担はない）。
これでパーサオブジェクトは外部を参照しなくなり、システム全体で参照になるのは
「結果の範囲 → 利用者の入力」の 1 本だけになる。

- テストは書いていない。コンビネータは今のところ `basic_json_parser` の private メンバで、外から構築できない。
  この性質は構造で保証している（`lit_parser` が参照型のメンバを持たない）。公開するときにテストを追加する

### (2) 文字列の認識とデコードを分離する

順序の変更: 当初の計画では「コンビネータを範囲に」を先にするつもりだったが、文字列のデコードが型付きの結果
（`many` の `vector<variant<string_view, basic_string>>` を連結）に依存していた。先にこちらを分けないと、
範囲化の時点で文字列のパースが壊れる。

変更:

- 文法の認識: 文字列本体のパーサは範囲（本体の `string_view`）だけを返す。エスケープの認識も範囲を返す
- デコード: `decode_string(body)` が別処理として `basic_string` を作る。本体にバックスラッシュが
  なければデコードせず、入力の範囲をそのままビジターに渡す（大半の文字列は割り当てなしで済む）
- エスケープの表は `unescape(s, out)` の 1 か所だけ。`out == nullptr` なら認識だけ、出力先があれば
  デコードもする。以前は認識とデコードが同じ関数に混ざっていた。`\uXXXX` を足すときはここだけ変えればよい

検討して見送ったこと:

- 認識用と変換用に別々のエスケープ表を持つ: 片方だけ更新して食い違う危険があるので却下
- 中身が不要なビジター（no-op ビジター）ではデコードを省く: ビジターが `on_string` を持つかどうかでは
  中身の要否を判定できない（no-op ビジターも `on_string` を持つ）。上の「エスケープがなければデコードしない」で
  実用上の費用はほぼ消えるので、ここで止めた

`noexcept` について: `parse_string` は `noexcept` のまま `basic_string` を割り当てる（変更前も同じ）。
メモリ不足は `std::terminate` になる。これは今回の変更範囲外として、パーサ全体の `noexcept` 方針と一緒に別途判断する。

テスト: 複数のエスケープと通常の文字が混在する文字列（両端がエスケープの場合も含む）、空文字列、
不正なエスケープ `\q`、閉じていないエスケープ `"\"` を追加した。

### (3) すべてのコンビネータがマッチ範囲を返す

`parse_result` をテンプレートから「`optional<string_view>` のマッチ範囲 + 残り」だけを持つ構造体に変えた。

- `alt`: 最初にマッチした側の結果をそのまま返す。`alt_result` / `variant` は削除
- `seq`: 両方を覆う範囲を返す。`tuple` は削除
- `many`: `vector` に集めず、覆った範囲を返す。さらに「内側のパーサが失敗した**または前進しなかった**ら止まる」
  ガードを入れた。`except` のクラッシュ修正で記録した教訓への対策で、`many(opt(x))` のような空マッチの組み合わせでも停止する
- `sep_by` / `ws` / 文字列本体: `seq` と `many` がそのまま範囲を返すので、範囲を組み直すコードが消えた
- `parse_result` の説明コメントに、ライフタイムの規則（結果はすべて呼び出し側の入力を指す）を明記した
- 副次効果: `many` が割り当てをしなくなったので、コンビネータの `noexcept` が嘘でなくなった
  （割り当てが残るのは文字列のデコードと DOM 構築だけ）
- `act(p, f)` はまだ追加していない。今のところ意味の処理はすべて `parse_*` からビジターを直接呼んでいて、
  コンビネータの中で意味の処理をしたい箇所がない。公開の準備をするとき、または必要になった時点で入れる

結果（GCC 14.2、-O0、5 回の中央値。「変更前」は `except` 修正直後・`lit` 変更前のヘッダ）:

| TU | 変更前 | 変更後 | 差 |
|---|---|---|---|
| 全 charT でパーサ + DOM ビジターを実体化 | 16.6 秒 | 9.2 秒 | −44% |
| `checked_string` 5 本の定数評価 | 3.6 秒 | 2.3 秒 | −36% |

（変更前の初回計測は 17.4 秒 / 3.6 秒。同じスナップショットを測り直した値を表に載せた。計測のぶれは 1 秒程度）

clang 18 + libstdc++ 14:

- 元の main 以来はじめて、テストスイート全体（`checked_string` の `static_assert` を含む）がコンパイルでき、全テストが通った
- `checked_string` 5 本の定数評価は 3.8 秒（3 回の中央値）。変更前は定数評価そのものができなかったので比較値はない
- EBCDIC テストは CMake のチェック（`-fexec-charset=IBM1047` が使えるか）で正しく除外された

これで「clang 18 + libstdc++ 14 では `checked_string` が機能しない」問題（2026-10-15 訂正エントリ）は解消した。
残っているのは `basic_json` 自体を定数式で作る場合（`variant<basic_string, …>`）だけで、これは
「constexpr パースの到達点」の問題として別に扱う。

## 2026-10-15: CI（GitHub Actions）を追加

目的: これまで CMake のテストは GCC でしか回っておらず、clang で `checked_string` が動かない問題に
main の時点から誰も気づかなかった。コンパイラ差を継続的に観測する仕組みとして CI を入れる。
コンパイラ差は記事の一次情報でもあるので、ログが自動で残ること自体に価値がある。

参考にしたもの: 作者の他リポジトリ `yk_util`（`run_test.yml`、2025-08）と `polyfill`（`test.yml`、2026-07）。
新しい `polyfill` の書き方に揃えた: `concurrency` による古い実行の取り消し、`paths` のアンカー、
`build/_deps`（Catch2）のキャッシュ、警告集計用 `iris-cpp/cpp-warning-notifier` と、それが正規表現で読む
ジョブ名の形式 `C++<版> <構成> (<処理系> <版> on <OS>)`。

行列（20 ジョブ）: {ubuntu 24.04, windows 2025-vs2026} × {Debug, Release} × {C++23, C++26} ×
{GCC 14, Clang 21 + libstdc++, Clang 21 + libc++, MSVC 2022 (toolset 14.44), MSVC 2026 (toolset 14.51)}。
OS と処理系の組は Linux に GCC / Clang、Windows に MSVC。

`polyfill` から意図的に変えた点:

- **Clang を libstdc++ と libc++ の両方で回す**: 定数評価で許されること（`std::variant`）と
  `from_chars` の対応範囲が標準ライブラリで違うため。下記の libc++ 18 の観測も参照
- **Windows でも Ninja を使う**: Visual Studio ジェネレータは、`setup-msvc-dev` で選んだ toolset の環境変数を
  無視して VS 既定の toolset でビルドする。そのため、行列の「MSVC 2022 (14.44)」の行が実際には別の toolset で
  ビルドされうる。Ninja なら環境の `cl` がそのまま使われる
- **追加のフラグは `CMAKE_CXX_FLAGS_INIT` で渡す**: `-DCMAKE_CXX_FLAGS=`（空）を渡すと、MSVC で CMake が
  既定で入れる `/EHsc` などが消える（`yk_util` が MSVC に `/EHsc` を明示していたのはこのためと推測）。
  `_INIT` にはプラットフォーム既定値が追記されるので消えない
- `permissions` に `contents: read` を明示（`polyfill` は `id-token: write` だけ）
- テストディレクトリ名の違い（`test/` ではなく `tests/`）と、`cmake/**` / `CMakeLists.txt` を監視対象に追加

CMake 側: `polyfill` の `YK_POLYFILL_CI` と同じ役割の `JSON20_CI` オプション（色付き診断）を追加した。
名前は既存の `JSON20_INSTALL` に揃えた。

EBCDIC テスト: 前のコミットで、有効化の判定を「`-fexec-charset=IBM1047` が通るか」から
「リテラルが実際に EBCDIC に翻訳されるか」（`'A' == 0xC1` を `static_assert`）に変えた。
MSVC は知らないオプションを警告（D9002）だけで無視するので、旧判定だと MSVC で EBCDIC テストが有効になり、
翻訳されないまま失敗するところだった。CI では GCC の行だけで EBCDIC テストが走る。

手元での検証（ワークフローと同じ CMake コマンドで実行）:

| 行 | 結果 |
|---|---|
| C++26 Release (GCC 14) | 5/5 通過（EBCDIC を含む） |
| C++23 Debug (Clang 18 + libstdc++) | 4/4 通過（EBCDIC は判定で除外） |
| C++26 Debug (Clang 18 + libc++ 18) | ビルド失敗（下記） |

ワークフローの YAML は actionlint で検査した。行列の展開（20 ジョブ）と、ジョブ名が警告集計の正規表現で
意図どおりに分解されることも確認した。参照している外部アクションのバージョン（checkout v7、cache v6、
setup-msvc-dev v4、cpp-warning-notifier v0.7）が実在することも確認した。

### コンパイラ挙動メモ — libc++ 18

- 浮動小数点の `std::from_chars` が**ない**。`double&` が `bool` に変換され、bool 誤用防止のために
  `= delete` された `from_chars(const char*, const char*, bool, int)` が選ばれて
  「call to deleted function」になる。エラーメッセージからは原因がわかりにくい。
  libc++ に `from_chars_floating_point.h` が入ったのは LLVM 20（release/19.x には無く、20.x にある）。
  CI の Clang 21 では問題にならないはず
- `std::variant<std::string, …>` を定数式で作れない点は libstdc++ と同じだが、**理由が違う**。
  libc++ 18 は `variant` 自体を「リテラル型でない」と判定する（libstdc++ は placement new を拒否）
- パーサ部分（`checked_string` の経路）は libc++ 18 でも定数評価できた。コンビネータを範囲に変えた効果
- libc++ 21 で `basic_json` を定数式で作れるかは、CI の結果で確認する（未解決の問いの 1 つ）

未検証・既知の限界:

- **MSVC ではまだ一度もビルドしていない**。初回の CI で失敗する可能性が高い。それ自体が記録すべき一次情報
- トリガーは `polyfill` と同じく main への push と main 向けの PR だけ。このブランチの push では走らない。
  最初の実行は PR を作ったとき
- テストが C++23（`<print>` など）を要求するので、ライブラリ本来の C++20 モードは CI で検査していない。
  テスト側を C++20 で書けるようにするかは別途判断する
- `run-notifier` ジョブは、CppWarningNotifier の GitHub App が json20 にインストールされていないと失敗する

## 2026-10-15: CI 初回実行（PR #1）の結果と修正

20 ジョブ中 6 ジョブだけが成功した。成功したのは GCC 14 の C++23（2 ジョブ）と Clang 21 + libc++ 21（4 ジョブ）。
失敗の原因は 4 つに分かれた。どれも、それまで GCC 14 と Clang 18 でしか確かめていなかったために見えていなかったもの。

| 失敗した行 | 原因 | 修正 |
|---|---|---|
| GCC 14 / C++26（2 ジョブ） | libstdc++ 14 が C++26 モードで EBCDIC に対応していない | json20 から `<ranges>` / `<locale>` を外した |
| Clang 21 + libstdc++ 14（4 ジョブ） | libstdc++ 14 の `std::bind_front` を clang 21 が再帰的な文法の中で拒否する | 戻り値型を明示したラムダ `bind_visitor` に置き換えた |
| MSVC 全 8 ジョブ | 従来型プリプロセッサが `__VA_ARGS__` を 1 つの引数として渡す | 引数の数による振り分けをやめ、引数の数ごとのマクロを直接呼ぶようにした |
| MSVC の Debug（4 ジョブ） | 別々の `string_view` のイテレータを混ぜて範囲を作っていた（こちらのバグ） | 長さから範囲を作る `consumed(str, rest)` に置き換えた |

### 訂正: 手元での「GCC 14 / C++26 で通過」は C++23 だった

CI 追加時のエントリに「C++26 Release (GCC 14) 5/5 通過」と書いたが、これは誤りだった。手元の CMake 3.28 は
GCC 14 の C++26 用フラグを知らず、`CMAKE_CXX_STANDARD=26` を渡しても**何も言わずに `-std=gnu++23` で
ビルドしていた**（同じ CMake でも clang 18 には `-std=gnu++26` が付いた）。`build.ninja` の `-std=` を
数えて発覚した。以降の手元検証は pip で入れた CMake 4.4 で行い、実際に付いた `-std=` を毎回確認している。

教訓: 「C++26 で確認した」と書くなら、実際にコンパイラへ渡ったフラグまで確認する。

### libstdc++ 14 は C++26 モードで EBCDIC に対応していない

C++26 モードの libstdc++ 14 では、`<format>` の内部（`bits/unicode.h` の `__charset_alias_match`）が
`<charconv>` の表 `__from_chars_alnum_to_val_table` を使う。この表は `'0' + i` を添字にして作られていて、
ASCII を前提にしている。EBCDIC では `'0'` が 0xF0 で、`char` は符号付きなので -16 になり、
「array subscript value '-16' is outside the bounds」で定数評価が失敗する。

- 単独で include しただけで失敗するヘッダ（GCC 14.2、`-std=gnu++26 -fexec-charset=IBM1047`）:
  `<format>`、`<ranges>`、`<locale>`。C++23 モードでは失敗しない
- json20 は `<ranges>` と `<locale>` を include していたので、EBCDIC + C++26 では json20.hpp を include するだけで
  コンパイルできなかった。EBCDIC をサポート対象にした以上、これは json20 の制限として扱う
- `<locale>` は一度も使っていなかった。`<ranges>` は `std::views::reverse` 4 か所のためだけだったので、
  逆順イテレータ + `std::find_if` に置き換えて両方外した。ranges のアルゴリズムは `<algorithm>` にある
- 将来 `<format>` などを使いたくなったら、EBCDIC + C++26 との両立を改めて判断する必要がある
- GCC 15 以降で直っているかは未確認（手元に GCC 15 がない）

### Clang 21 は libstdc++ 14 の `std::bind_front` を再帰的な文法の中で拒否する

libstdc++ 14 の `_Bind_front::operator()` は、explicit object parameter（deducing this）と `std::forward_like` で
書かれている。json20 の文法は再帰していて（値 → 配列 → 値、値 → オブジェクト → 値）、`bind_front` で包んだパーサを
組み合わせると、clang 21 は次のエラーを出す:

```
function 'forward_like<...>' with deduced return type cannot be used before it is defined
no matching function for call to 'bind_front'
```

- clang 18 は同じコードを受け付けていた。GCC 14 も受け付ける。clang 21 + libc++ 21 も問題ない
  （libc++ の `bind_front` は別の実装）
- 公式のビルド済み Clang 21.1.8（GitHub のリリースアーカイブ）を手元に入れて再現した。修正前は parse.cpp で
  21 件、main.cpp で 4 件のエラーで、CI と一致した。修正後は 0 件。apt.llvm.org はこの作業環境から
  接続できなかったので、2 GB のアーカイブを途中再開つきでダウンロードした
- 修正: `bind_visitor(parser, vis)` が「戻り値型 `parse_result` を明示したラムダ」を返す。中で呼ぶのは普通の
  関数ポインタなので、再帰の中に推論待ちの型が現れない。`<functional>` も不要になった。`std::tuple` /
  `std::make_from_tuple` は `<functional>` 経由で使えていただけだったので、`<tuple>` を直接 include した

### MSVC: 従来型プリプロセッサと `__VA_ARGS__`

ビジター呼び出しのマクロ `YK_JSON20_INVOKE_VISITOR(...)` は、`SELECTER(__VA_ARGS__, ARG4, ARG3, ARG2)` で
引数の数を数えて振り分ける、よくある書き方だった。MSVC の既定の（従来型）プリプロセッサは、`__VA_ARGS__` を
別のマクロに渡すとき 1 つの引数として扱う。そのため振り分けが常に間違い（warning C4003）、12 か所すべてで
「'on_null': undeclared identifier」などの構文エラーになった。`/Zc:preprocessor` を付ければ規格準拠の
プリプロセッサになるが、既定では無効。

検討した選択肢:

1. `EXPAND(x) x` で 1 段かませる定番の回避策。MSVC で動くことは広く知られているが、手元で確かめる手段がない
   （clang の `-fms-compatibility` はこの挙動を再現しないことを確認した）
2. `/Zc:preprocessor` を必須にする（CMake の INTERFACE オプションで付ける）。ヘッダオンリーのライブラリが
   利用者にコンパイラオプションを強制することになるので却下
3. **引数の数による振り分けをやめ、呼び出し箇所で `_ARG2` / `_ARG3` / `_ARG4` を直接呼ぶ（採用）**。
   可変長引数を使わないので、どのプリプロセッサでも同じに展開される。手元で検証できない以上、
   一番確実な方法を選んだ

### MSVC の Debug: 別々の `string_view` のイテレータを混ぜていた

パーサはマッチ範囲を `string_view{str.begin(), rest.begin()}` で作っていた（7 か所）。`str` と `rest` は
同じ文字配列を指す別々の `string_view` オブジェクト。MSVC のデバッグ用 `string_view` イテレータは、
`operator-` で両者が同じビューのものかを検査し、違えば `_CrtDbgReport` を呼ぶ。これは constexpr ではないので、
定数評価がすべて失敗した（`static_assert` で C2131、`checked_string` で C7595）。Release では検査がないので
この失敗は出ない。

- これは MSVC の過剰な検査ではなく、こちらのバグ。標準は、別の範囲に属するイテレータ同士の差に意味を与えていない
  （たまたま同じ配列を指していても）
- 修正: `consumed(str, rest)` = `str.substr(0, str.size() - rest.size())`。「`rest` は `str` の末尾部分」という
  `parse_result` の不変条件だけに依存し、イテレータを使わない
- 手元では再現できない（libstdc++ と libc++ の `string_view` イテレータはただのポインタ）

### 未解決の問いへの答え: libc++ 21 では `basic_json` を定数式で作れる

Clang 21 + libc++ 21 では、`static_assert(json{}.get_kind() == json_value_kind::null)` などの
`basic_json` の定数式構築が**通った**。比較:

| 標準ライブラリ | `variant<basic_string, …>` の定数式構築 |
|---|---|
| libstdc++ 14（GCC 14） | 通る |
| libstdc++ 14（Clang 18 / 21） | 失敗（placement new を拒否） |
| libc++ 18 | 失敗（`variant` がリテラル型でない） |
| libc++ 21 | **通る** |

「constexpr パースの到達点」を考えるときの前提が変わる: Clang でも libc++ を使えば、DOM の定数式構築まで届く。

## 2026-10-15: 調査 — 数値変換を自前で実装するためのアルゴリズム（方針は未決定）

発端: Codex のレビュー（PR #1）で、EBCDIC では数値の読み出しが動かないと指摘された（整数はコンパイルできず、
浮動小数点は例外）。作者の判断で、数値変換を自前で実装することにした。

json20 固有の条件:

1. **charT に依存しない**（char / wchar_t / char8_t / char16_t / char32_t）
2. **文字コードに依存しない**（EBCDIC を含む）。数字は翻訳されたリテラル `'0'` からの差で扱う。
   `'0'`〜`'9'` が連続していることは規格が保証している
3. **C++20 で constexpr にできる**（`checked_string` の先にある「定数式でのパース」のため）
4. 外部依存なし。数値表などは自前のスクリプトで生成する

### 文字列 → 浮動小数点（パース）

- **Clinger の高速経路**: 仮数が 2^53 以下で、10 の指数の絶対値が 22 以下なら、double の乗算 / 除算 1 回で
  正確に求まる。JSON の数値の大半はここで終わる
- **Eisel-Lemire**（Lemire, "Number Parsing at a Gigabyte per Second", 2021）: 5 の累乗の 128 ビット近似表
  （約 650 項目、約 10 KB）と 64×64→128 ビットの乗算 1〜2 回。GCC の libstdc++、LLVM の libc++ の
  `from_chars` が採用している（fast_float の README による）
- **Mushtak と Lemire, "Fast Number Parsing Without Fallback"（2023）**: 有効数字が 19 桁以内（64 ビットに
  収まる）なら、Eisel-Lemire の 128 ビット積は**常に**正しい丸めの判定に十分であることを証明した。
  fast_float のソースにも「The computed 'product' is always sufficient」とこの論文を引いたコメントがある
- 補正が必要なのは 20 桁以上の入力だけ（切り捨てた仮数 w と w+1 で結果が変わる場合）。fast_float はここで
  多倍長整数による桁の比較を行う（`bigint.h` + `digit_comparison.h` で約 1,100 行）
- **fast_float は C++20 で constexpr、UTF-16/32 にも対応しているが、EBCDIC では動かない**。8 桁を一度に
  処理する高速化（SWAR）で `0x3030303030303030`（ASCII の `'0'` を 8 個並べた値）を直書きしている
  （`ascii_number.h`）。仮に取り込むとしても、この部分は書き換えが必要

### 浮動小数点 → 最短文字列（シリアライズ）

| アルゴリズム | 特徴 | json20 から見た評価 |
|---|---|---|
| Ryū（Adams, 2018） | 表が大きい。MSVC STL / libstdc++ の `to_chars` が採用 | 後発に速度で劣る |
| Schubfach（Giulietti, 2017-2018） | 証明つき。Java 19 以降が採用。Dragonbox と Żmij の土台 | 単純で移植しやすい |
| Dragonbox（Jeon, 2020） | 最短・正しい丸め・往復保証。**C++20 ですべての関数が constexpr**（README）。表は通常 9,904 バイト、圧縮版なら 584 バイト（約 20% 遅い）。数字の表が文字リテラル `{'0', '0'}, …` なので EBCDIC でも翻訳される | 第一候補。ただし本体は方針指定（policy）のテンプレートが多く、作者自身が移植には簡易版（約 1,500 行）を勧めている |
| Żmij（Zverovich ほか, 2025-2026） | 最速（README によると Ryū の 3.8〜7 倍）。Lean で形式検証済み。Schubfach 系 | 速さの多くは SIMD と、文字を 64 ビット整数に詰める処理（バイト順と ASCII の前提）から来ている。charT・EBCDIC・constexpr の条件ではその部分を使えない |
| yy（Guo / ibireme） | Schubfach 系で、10 の累乗との乗算が 1 回で済む | 独立した検証を確認できなかった |

### 整数

- パース: 1 桁ずつのループ（`c - '0'`）と桁あふれの検査。SWAR は ASCII 前提かつ文字幅ごとの実装が必要なので
  使わない。JSON の整数は高々 20 桁なので、ループの費用は小さい
- 出力: 2 桁ずつの表を文字列リテラル `"00010203…99"` で持てば、`YK_JSON20_WIDEN_STRING` で charT ごとに
  用意でき、EBCDIC でもコンパイラが翻訳する

### 表を共有できる可能性

Eisel-Lemire が使う 5^q の正規化した 128 ビット近似と、Schubfach / Dragonbox が使う 10^k の近似は、
2 の累乗の違いを除けば同じ数（10^k = 5^k × 2^k）なので、1 つの表を共有できる可能性がある。ただし丸め方向
（切り捨てか切り上げか）の要件が違う項目があるので、実装時に確かめる必要がある。

### 情報源についての注意

この作業環境からは GitHub 以外のサイト（arXiv、著者のブログ）に接続できなかった。上の記述は各実装の README と
ソースコード（fast_float、Dragonbox、Żmij）、および検索結果の要約に基づく。2026 年の比較論文
（arXiv 2603.06581、"Converting Binary Floating-Point Numbers to Shortest Decimal Strings: An Experimental Review"）
の本文は未読。

## 2026-10-15: 設計判断 — 整数の変換を自前で実装（浮動小数点は次の PR で Dragonbox）

作者の判断:

- 数値変換は自前で実装する。浮動小数点の出力は **Dragonbox** を採用する
- この PR（#1）では**整数だけ**を実装する。浮動小数点（Dragonbox による出力と Eisel-Lemire によるパース）は
  規模が大きく、乱数による大量の往復テストも要るので、別の PR に分ける

整数の実装:

- パース: 1 桁ずつのループで、各文字を翻訳済みのリテラル `'0'` からの差（0〜9）として読む。符号付きの型では
  先頭の `-` を受け付ける。最も小さい負の値の絶対値は最大値より 1 大きいので、上限をそれに合わせてから
  桁あふれを検査する（`value > (limit - digit) / 10` なら範囲外）。数字がない場合と範囲外の場合は、これまでどおり
  `std::invalid_argument` を投げる（例外の型は変えない）。最初の数字でない文字で止まるのは `from_chars` と同じ
- 出力: 2 桁ずつの表を文字列リテラル `"000102…99"` で持ち、`YK_JSON20_WIDEN_STRING` で charT ごとに用意する。
  リテラルなので、EBCDIC でもコンパイラが数字を翻訳する。符号付きの型の最小値は、符号なしの型に移してから
  `0 - value` で絶対値を作る（`-x` は桁あふれするため）
- bool は対象外（`!std::same_as<T, bool>` で除外）。浮動小数点は次の PR まで `char` 専用の
  `std::from_chars` / `std::to_chars` のまま

効果:

- EBCDIC（`-fexec-charset=IBM1047`）で、整数の読み出しと書き出しがコンパイルでき、値も正しい
  （tests/ebcdic.cpp に `static_assert` で追加。修正前は libstdc++ の `<charconv>` の中でコンパイルエラー）
- **wchar_t / char8_t / char16_t / char32_t でも整数を扱えるようになった**。これまでは serializer /
  deserializer が `char` にしか特殊化されておらず、`wjson(42)` も `as_signed_integer` もコンパイルできなかった
- **C++20 で constexpr**（`from_chars` の整数版が constexpr になったのは C++23 から）。
  `static_assert(serializer<int, char>::serialize(-1234) == "-1234")` のように定数式で使える

見送ったこと:

- 8 桁を一度に処理する高速化（SWAR）: ASCII の値を直書きする必要があり、文字幅ごとに実装も要る。JSON の整数は
  高々 20 桁なので、1 桁ずつのループとの差は小さい
- 例外の型を `std::out_of_range` に変える: 範囲外を表す型としてはそちらが正確だが、挙動の変更になるので別の判断とする

MSVC への配慮: 定数式の中で初期化していない配列を使うこと（C++20 で許可）を MSVC が受け付けるか手元で確かめられない
ので、出力用のバッファは `{}` で値初期化した。
