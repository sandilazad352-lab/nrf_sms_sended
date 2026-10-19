# বুক কাউন্টার মোবাইল অ্যাপ: ডেভেলপার গাইড

এই নথিটি Book Counter NRF52 ফার্মওয়্যারের জন্য Android/iOS অ্যাপ তৈরির নির্দেশিকা। এই রিপোজিটরিতে শুধু ডিভাইসের ফার্মওয়্যার আছে; মোবাইল অ্যাপ, ব্যাকএন্ড, অ্যাকাউন্ট ব্যবস্থা বা push-notification server নেই।

## অ্যাপের উদ্দেশ্য

Bluetooth Low Energy (BLE) দিয়ে Book Counter খুঁজে সংযোগ করা, IR সেন্সরের মোট গণনা ও ব্যাটারি দেখানো, proximity alert সেট করা, গণনার sync অবস্থা সামলানো এবং অনুমোদিত ব্যবহারকারীকে ডিভাইসের গণনা reset করতে দেওয়া।

ডিভাইসের count হলো ক্রমবর্ধমান মোট সংখ্যা। Reset করলে ডিভাইসের count শূন্য হবে; অ্যাপের local history বা backend-এর রেকর্ড মুছে যাবে না।

## প্রথম সংস্করণের কাজ

- ডিভাইস খোঁজা, connect/disconnect, pairing এবং পুনরায় সংযোগ।
- বর্তমান count ও battery status দেখানো।
- সংযুক্ত থাকা অবস্থায় count, battery ও event notification গ্রহণ।
- ব্যবহারকারীর RSSI boundary ও proximity alert পছন্দ সংরক্ষণ।
- Local count sync অবস্থা রাখা; পরে backend upload যোগ করার সুযোগ রাখা।
- ব্যবহারকারীর নিশ্চিতকরণের পর reset command পাঠানো এবং ফল যাচাই।
- Permission, connection, pending write এবং error-এর পরিষ্কার অবস্থা দেখানো।

অ্যাপের framework এবং ন্যূনতম Android/iOS সংস্করণ প্রকল্পের চাহিদা অনুযায়ী বেছে নিন। BLE কাজকে আলাদা service/repository-তে রাখুন, যাতে screen সরাসরি BLE library-এর ওপর নির্ভর না করে। এমন রক্ষণাবেক্ষিত BLE library ব্যবহার করুন যাতে scan, GATT notification, encrypted write এবং প্রয়োজনীয় OS background behavior সমর্থিত।

## অ্যাপের প্রধান স্ক্রিন

1. **সেটআপ ও ডিভাইস তালিকা:** Bluetooth permission কেন দরকার বুঝিয়ে বলুন, কাছের Book Counter খুঁজুন, connection state দেখান এবং ডিভাইস বেছে pairing করতে দিন।
2. **কাউন্টার:** বর্তমান মোট সংখ্যা, battery percent বা “পাওয়া যায়নি”, সর্বশেষ update সময়, connection ও sync অবস্থা দেখান। Notification এলে UI update করুন; reconnect বা অ্যাপ foreground-এ এলে read করে মিলিয়ে নিন।
3. **Alert:** ব্যবহারকারীকে near/far RSSI threshold এবং proximity alert চালু/বন্ধ করতে দিন। RSSI দূরত্বের নির্ভুল মাপ নয়—এটি radio signal-এর আনুমানিক মান—এ কথা জানান।
4. **Sync ও ইতিহাস:** সর্বশেষ সফলভাবে সংরক্ষিত/sync করা count ও সময় এবং retry বোতাম দেখান। ইতিহাসকে অ্যাপ/backend-এর রেকর্ড হিসেবে চিহ্নিত করুন; ফার্মওয়্যার প্রতিটি passage-এর আলাদা log দেয় না।
5. **ডিভাইস সেটিংস:** Reconnect, অ্যাপের স্থানীয় nickname, firmware/protocol তথ্য (থাকলে), সহায়তা এবং “ডিভাইসের count reset” কাজ রাখুন।

Reset-এর আগে নির্বাচিত ডিভাইসের নামসহ confirmation দেখান এবং জানান যে ডিভাইসের count ও sync acknowledgement শূন্য হবে। BLE write চলাকালে বারবার চাপা বন্ধ রাখুন। সফল হলে আবার count ও acknowledgement পড়ে দুটোই শূন্য নিশ্চিত করার পরেই সম্পন্ন দেখান। Write response হারিয়ে গেলে retry দেওয়ার আগে ডিভাইসের মান পড়ুন।

## BLE প্রোটোকল

বিজ্ঞাপনে ডিভাইসের নাম: `Book Counter`  
প্রধান service UUID: `FFF0`

Scan করার সময় service UUID এবং/অথবা নাম ব্যবহার করুন। OS নাম ছোট করে দেখাতে পারে, তাই সম্ভব হলে service UUID-কে অগ্রাধিকার দিন। নিচের সব multi-byte integer little-endian। Connect করার পর service ও characteristic খুঁজুন, প্রয়োজনীয় notification চালু করুন এবং encrypted write-এর আগে OS-কে BLE link encrypt/pair করতে দিন।

| Characteristic | UUID | Access | ডেটা |
| --- | --- | --- | --- |
| Count | `FFF1` | Read, notify | 4-byte unsigned মোট count |
| Battery | `FFF2` | Read, notify | 1 byte: `0..100` percent; `FF` মানে পাওয়া যায়নি |
| Event | `FFF3` | Notify | 8 byte: event code `u8`, battery `u8`, event sequence `u16`, count `u32` |
| Settings | `FFF4` | Read, write (encrypted) | 2 byte: signed RSSI dBm `i8`, flags `u8` |
| Sync acknowledgement | `FFF5` | Read, write (encrypted) | 4-byte unsigned acknowledged count |
| Count reset | `FFF6` | Write (encrypted) | নির্দিষ্ট 4 byte `52 53 54 31` (ASCII `RST1`) |

### Event code

| Code | অর্থ |
| --- | --- |
| `1` | IR beam passage গণনা হয়েছে |
| `2` | সংযুক্ত ফোন নির্ধারিত RSSI boundary-এর কাছে এসেছে |
| `3` | সংযুক্ত ফোন boundary থেকে দূরে গেছে |
| `4` | Battery অনুমানের মান firmware warning threshold-এ বা তার নিচে |
| `5` | Counter reset সম্পন্ন; event-এর count শূন্য |

Event packet-এর count হলো event-এর সময়কার মোট count। UI-তে এই মান দিয়ে update করুন, আগের count-এর সঙ্গে যোগ করবেন না। Event বাদ পড়তে বা পুনরাবৃত্তি হতে পারে; ডিভাইস থেকে পড়া cumulative count-ই নির্ভরযোগ্য মান।

### Settings-এর encoding

- RSSI threshold হলো signed two's-complement byte। ফার্মওয়্যার `-90` থেকে `-35` dBm গ্রহণ করে।
- Flags-এর bit 0 proximity event চালু করে; বাকি bit শূন্য লিখুন।
- উদাহরণ: `-65 dBm`, proximity চালু = `BF 01`।
- ডিভাইসে সব ফোনের জন্য এক সেট shared setting থাকে। অ্যাপে প্রতিটি ব্যবহারকারীর পছন্দ আলাদা করে রাখুন এবং connect-এর পর লিখুন। যে অ্যাপ সর্বশেষ লিখবে, তার setting কার্যকর হবে।

### Reset-এর নিয়ম

ব্যবহারকারীর নিশ্চিতকরণ ছাড়া reset পাঠাবেন না। `FFF6`-এ ঠিক `52 53 54 31` byte লিখুন। ফার্মওয়্যার count ও sync acknowledgement শূন্য করে, persistence-এর জন্য সংরক্ষণের কাজ চিহ্নিত করে, পড়ার মান update করে, count notification পাঠায় এবং event notification চালু থাকলে code `5` পাঠায়। এরপর `FFF1` ও `FFF5` পড়ে শূন্য নিশ্চিত করুন।

Reset proximity setting বা BLE bond মুছে না। অ্যাপের ডেটা, server history বা অন্য ফোনের রেকর্ডও মুছে না। Reset-এর সময়/নতুন baseline অ্যাপ বা backend-এ রাখুন, যাতে পরে count `3` হলে সেটিকে ভুল করে count কমে যাওয়া বা data loss মনে না হয়।

## Connect ও sync প্রক্রিয়া

Connect হলে:

1. `FFF0` service এবং characteristic খুঁজুন।
2. `FFF1`, `FFF2` ও `FFF3` notification চালু করুন।
3. Count, battery, settings ও acknowledgement পড়ুন।
4. বর্তমান ব্যবহারকারীর সংরক্ষিত setting ডিভাইসের setting থেকে আলাদা হলে `FFF4`-এ লিখুন।
5. Count local-এ সংরক্ষণ করুন। Backend থাকলে enrolled app/backend device ID এবং স্থিতিশীল request key দিয়ে idempotent upload করুন।
6. নির্বাচিত local/backend sync ধাপ সফল হলেই সেই count `FFF5`-এ লিখুন; পরে পড়ে নিশ্চিত করুন।

অ্যাপ আবার foreground-এ এলে সম্ভব হলে reconnect বা count read করুন। শুধু notification-কে সত্যের একমাত্র উৎস ভাববেন না। Disconnect হলে সর্বশেষ local reading ও তার সময় রাখুন। সীমিত বিরতিতে retry করুন, এবং sync এমনভাবে লিখুন যেন একই কাজ পুনরায় করলেও সমস্যা না হয়।

Acknowledgement বোঝায় অ্যাপ/backend ওই count পর্যন্ত reading সামলেছে; এটি ডিভাইসের count মুছে না। Backend-এর বড় count ডিভাইসে লিখবেন না: ফার্মওয়্যারে count import/restore command নেই।

## Notification ও background সীমাবদ্ধতা

ফার্মওয়্যার সংযোগ থাকা অবস্থায় GATT notification পাঠায়। ব্যবহারকারী অনুমতি দিলে অ্যাপ event পেয়ে Android/iOS-এর local notification দেখাতে পারে। ফার্মওয়্যার নিজে ফোনে push notification পাঠাতে পারে না; BLE advertisement একা notification দেখায় না।

Background scan ও BLE connection OS, সংস্করণ এবং permission অনুযায়ী বদলে যায়। Alert পেতে অ্যাপকে সংযুক্ত থাকতে বা background-এ চলার অনুমতি লাগতে পারে—এটি পরিষ্কারভাবে জানান। অ্যাপের প্রয়োজনীয় Bluetooth/nearby-device, প্রযোজ্য Android সংস্করণে location, এবং notification permission-ই চান। OS scan বন্ধ করলে বা ডিভাইস disconnect থাকলে alert নিশ্চিত বলে দাবি করবেন না।

বর্তমান firmware advertisement manufacturer block:

```text
FF FF | 42 43 | 01 | eventActive | eventSequence:u16 LE
```

সঠিক count advertisement-এ নেই। `FF FF` বর্তমান firmware-এ test company identifier; বাণিজ্যিক ব্যবহারের আগে বরাদ্দকৃত identifier দিয়ে বদলাতে হবে। 16-bit sequence ঘুরে আবার শুরু হয় এবং reboot-এ reset হয়; একে একা স্থায়ী event ID হিসেবে ব্যবহার করবেন না।

## অ্যাপের ডেটা মডেল

কমপক্ষে এগুলো সংরক্ষণ করুন:

- **ব্যবহারকারীর পছন্দ:** RSSI threshold, proximity চালু/বন্ধ, count notification চালু/বন্ধ।
- **ডিভাইস রেকর্ড:** অ্যাপ-নির্ধারিত/enrolled ID, স্থানীয় nickname, সর্বশেষ দেখা count, সর্বশেষ acknowledgement, সর্বশেষ sync ও connection সময়, reset-এর সময়গুলো।
- **ঐচ্ছিক backend reading:** enrolled device ID, cumulative count, গ্রহণের সময়, idempotency key এবং reset epoch/boundary।

BLE MAC address-কে স্থায়ী পরিচয় হিসেবে ধরবেন না; address পরিবর্তিত/randomized হতে পারে। কোনো passage নির্দিষ্ট ব্যক্তির নামে দেখাবেন না—হার্ডওয়্যার শুধু aggregate count দেয়।

## ত্রুটি সামলানো

- ডিভাইস না পাওয়া গেলে বা connect-এর সময় হারিয়ে গেলে retry এবং সর্বশেষ জানা ডেটা/সময় দেখান।
- Pairing বা encrypted write ব্যর্থ হলে pairing-এর প্রয়োজন বুঝিয়ে retry দিন।
- Notification setup ব্যর্থ হলেও read/refresh চালু রাখুন এবং live update বন্ধ আছে জানান।
- Settings write ব্যর্থ হলে ব্যবহারকারীর পছন্দ অ্যাপে রাখুন এবং device update pending দেখান।
- Sync/backend upload ব্যর্থ হলে acknowledgement বাড়াবেন না; local data রেখে retry দিন।
- Battery `FF` হলে “পাওয়া যায়নি” দেখান, 0% নয়।
- Count `UINT32_MAX` হলে মানটি দেখান; firmware count wrap না করে saturate করে।
- Reset write-এর ফল অনিশ্চিত হলে retry-এর আগে count/acknowledgement পড়ুন।
- আগের reading-এর চেয়ে count কম হলে reset boundary আছে কি না দেখুন; চুপিসারে merge বা overwrite করবেন না।

## কাজ সম্পন্নের যাচাই তালিকা

- নির্বাচিত Android/iOS সংস্করণে অ্যাপ ডিভাইস খুঁজে connect করতে পারে।
- Pairing এবং `FFF4`, `FFF5`, `FFF6`-এ encrypted write কাজ করে।
- Count ও battery ঠিকভাবে পড়ে; battery unavailable-কে খালি battery থেকে আলাদা দেখায়।
- Count notification-এ UI update হয় এবং reconnect-এর পর read করে পুনরুদ্ধার হয়।
- ব্যবহারকারীর setting অ্যাপে থাকে এবং প্রতিবার connect-এ প্রয়োগ হয়।
- নির্বাচিত sync save/upload সফল না হওয়া পর্যন্ত acknowledgement বাড়ে না।
- Reset confirmation চায়, নির্দিষ্ট token লেখে, count ও acknowledgement শূন্য যাচাই করে এবং অ্যাপ/server history রাখে।
- Duplicate বা বাদ পড়া notification-এ count/sync state নষ্ট হয় না।
- Offline, disconnected, pending এবং permission সীমাবদ্ধতার অবস্থা UI-তে সঠিকভাবে দেখা যায়।
- Passage-ভিত্তিক ইতিহাস, ব্যক্তিকে শনাক্ত করা, RSSI থেকে নির্ভুল দূরত্ব বা নিশ্চিত background alert-এর দাবি করা হয় না।

## ফার্মওয়্যার রেফারেন্স

বিদ্যমান protocol handoff-এর জন্য `APP_HANDOFF.md`, BLE আচরণের জন্য `src/BleCounterService.cpp`, UUID-এর জন্য `include/AppConfig.h` এবং persistent state-এর জন্য `src/CounterStorage.cpp` দেখুন। Firmware project root থেকে build command: `python -m platformio run`।
