"""GLYPH bridge: chat with an LLM, and the GLYPH-LCD face listens, thinks and talks it out.

  you type (terminal or Telegram)
      -> face gets ":typing", then "> your message"  (it listens and reacts)
      -> Claude streams a reply; tokens go to the face as they arrive (it talks at token pace)
      -> the reply also comes back to you in the terminal / Telegram chat

Where the face is:
  --port /dev/ttyUSB0          the real board over USB (pip install pyserial)
  --lab http://HOST:8602       the Freenove Kit Lab in your browser (no hardware needed)

Where you chat from:
  (default)                    this terminal
  --telegram                   a Telegram bot; set TELEGRAM_BOT_TOKEN (optionally TELEGRAM_CHAT_ID to lock it to you)

Brain:
  (default) Claude             pip install anthropic; uses ANTHROPIC_API_KEY or an `ant auth login` profile
  --echo                       no LLM: repeats you back, for wiring tests

Examples:
  python3 glyph_bridge.py --lab http://192.168.1.119:8602
  python3 glyph_bridge.py --port /dev/ttyUSB0 --telegram
"""
import argparse
import json
import os
import sys
import time
import unicodedata
import urllib.parse
import urllib.request

MODEL = "claude-opus-5-5"
SYSTEM = (
    "You are Glyph, a small, warm, expressive face living on a 16x2 LCD screen with a buzzer voice. "
    "Reply in one or two short sentences of plain text: no emoji, no markdown, no lists. "
    "Show feelings in your words (happy, sorry, wow, hmm, love) because your face reacts to them. "
    "Use CAPS on a word only to really shout it."
)
HISTORY_TURNS = 12               # keep the chat short: the face only needs recent context


def ascii_only(s):
    """The LCD speaks ASCII: fold accents, drop the rest, flatten newlines."""
    s = unicodedata.normalize("NFKD", s).encode("ascii", "ignore").decode()
    return s.replace("\r", " ").replace("\n", " ")


# ---- face sinks: the board over serial, or the lab over HTTP ----
class SerialFace:
    def __init__(self, port):
        import serial            # pyserial
        self.s = serial.Serial(port, 9600, timeout=0)
        time.sleep(2.0)          # opening the port resets the Uno; let it boot

    def send(self, text):
        self.s.write(text.encode("ascii", "ignore"))
        self.s.flush()


class LabFace:
    def __init__(self, url):
        self.url = url.rstrip("/") + "/serial"

    def send(self, text):
        req = urllib.request.Request(self.url, data=text.encode(), method="POST")
        urllib.request.urlopen(req, timeout=5).read()


# ---- brains ----
class ClaudeBrain:
    def __init__(self):
        import anthropic
        self.anthropic = anthropic
        self.client = anthropic.Anthropic()
        self.history = []

    def reply(self, text):
        """Yield reply text chunks as they stream."""
        self.history.append({"role": "user", "content": text})
        self.history = self.history[-HISTORY_TURNS * 2:]
        if self.history[0]["role"] != "user":
            self.history = self.history[1:]
        out = []
        try:
            with self.client.beta.messages.stream(
                model=MODEL,
                max_tokens=2000,
                system=SYSTEM,
                messages=self.history,
                output_config={"effort": "low"},         # chat: fast and short beats deep
                betas=["server-side-fallback-2026-07-01"],
                fallbacks="default",                      # a declined request re-runs on a fallback model
            ) as stream:
                for chunk in stream.text_stream:
                    out.append(chunk)
                    yield chunk
                final = stream.get_final_message()
            if final.stop_reason == "refusal":
                out = ["Hmm, I can't talk about that one."]
                yield " " + out[0]
        except self.anthropic.RateLimitError:
            out = ["Whoa, too many words at once. Give me a second?"]
            yield out[0]
        except self.anthropic.APIStatusError as e:
            out = [f"Something went wrong on my end ({e.status_code})."]
            yield out[0]
        except self.anthropic.APIConnectionError:
            out = ["I can't reach my brain right now. Is the internet okay?"]
            yield out[0]
        self.history.append({"role": "assistant", "content": "".join(out) or "..."})


class EchoBrain:
    def reply(self, text):
        for word in f"You said: {text}".split(" "):
            time.sleep(0.08)     # pretend to stream
            yield word + " "


# ---- chat sources: terminal or Telegram ----
def terminal_messages():
    print("GLYPH bridge: type to chat, Ctrl+C to quit. Lines starting with ':' go straight to the face.")
    while True:
        try:
            line = input("you > ").strip()
        except EOFError:
            return
        if line:
            yield line, None


class Telegram:
    def __init__(self, token, chat_id=None):
        self.api = f"https://api.telegram.org/bot{token}/"
        self.chat_id = str(chat_id) if chat_id else None
        self.offset = 0

    def call(self, method, **params):
        data = urllib.parse.urlencode(params).encode()
        with urllib.request.urlopen(self.api + method, data=data, timeout=40) as r:
            return json.load(r)

    def messages(self):
        print("GLYPH bridge: listening on Telegram.")
        while True:
            try:
                res = self.call("getUpdates", offset=self.offset, timeout=30)
            except OSError as e:
                print("telegram:", e, file=sys.stderr)
                time.sleep(5)
                continue
            for upd in res.get("result", []):
                self.offset = upd["update_id"] + 1
                msg = upd.get("message") or {}
                chat = str(msg.get("chat", {}).get("id", ""))
                if msg.get("text") and (not self.chat_id or chat == self.chat_id):
                    yield msg["text"], chat

    def send(self, chat, text):
        self.call("sendMessage", chat_id=chat, text=text)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    where = ap.add_mutually_exclusive_group(required=True)
    where.add_argument("--port", help="serial port of the board, e.g. /dev/ttyUSB0 or COM3")
    where.add_argument("--lab", help="Kit Lab URL, e.g. http://192.168.1.119:8602")
    ap.add_argument("--telegram", action="store_true", help="chat through a Telegram bot (TELEGRAM_BOT_TOKEN)")
    ap.add_argument("--echo", action="store_true", help="no LLM, just echo (wiring test)")
    ap.add_argument("--mode", help="face profile to start in: basic, kawaii, robot, grumpy, brows")
    args = ap.parse_args()

    face = SerialFace(args.port) if args.port else LabFace(args.lab)
    brain = EchoBrain() if args.echo else ClaudeBrain()
    if args.mode:
        face.send(f":mode {args.mode}\n")

    tg = None
    if args.telegram:
        token = os.environ.get("TELEGRAM_BOT_TOKEN")
        if not token:
            sys.exit("Set TELEGRAM_BOT_TOKEN (from @BotFather) to use --telegram.")
        tg = Telegram(token, os.environ.get("TELEGRAM_CHAT_ID"))
    source = tg.messages() if tg else terminal_messages()

    try:
        for text, chat in source:
            if text.startswith(":"):                    # direct face command, e.g. :mode kawaii
                face.send(text + "\n")
                continue
            face.send(":typing\n")
            face.send("> " + ascii_only(text)[:120] + "\n")
            if not tg:
                print("glyph > ", end="", flush=True)
            reply = []
            for chunk in brain.reply(text):
                reply.append(chunk)
                face.send(ascii_only(chunk))
                if not tg:
                    print(chunk, end="", flush=True)
            face.send("\n")
            if tg:
                tg.send(chat, "".join(reply).strip() or "...")
            else:
                print()
    except KeyboardInterrupt:
        print()


if __name__ == "__main__":
    main()
