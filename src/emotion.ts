// Text → expression heuristic. Local, zero-cost, no AI call.

const EMOJI: [RegExp, string][] = [
  [/[😄😀😊🙂😁😆🥳]/u, 'happy'],
  [/[😢😭😞☹️🥺]/u, 'sad'],
  [/[🤔💭]/u, 'think'],
  [/[😲😮😱🤯]/u, 'shock'],
  [/[😍🥰❤️♥💕]/u, 'love'],
  [/[😵🥴😖]/u, 'dizzy'],
  [/[😉😏]/u, 'wink'],
];

export function mirrorEmoji(text: string): string | null {
  for (const [re, name] of EMOJI) if (re.test(text)) return name;
  return null;
}

// Scan a window of streamed text for emotional cues. Returns expression name or null (= keep talking).
export function analyze(text: string): string | null {
  const t = text.slice(-240);
  const lower = t.toLowerCase();
  if (/\berror\b|\bfailed\b|\bexception\b|\bsorry\b|\bcannot\b|\bunable\b/.test(lower)) return 'error';
  if (/!{2,}|\bwow\b|\bincredible\b|\bamazing\b/.test(lower) || /\b[A-Z]{4,}\b/.test(t)) return 'shock';
  if (/\blove\b|\badore\b|♥|❤/.test(lower)) return 'love';
  if (/\bgreat\b|\bawesome\b|\bperfect\b|\bnice\b|\bcongrats\b|\bthanks\b/.test(lower)) return 'happy';
  if (/\bhmm\b|\bnot sure\b|\bmaybe\b|\bunclear\b|\?$/.test(lower)) return 'confused';
  // long dry list → stay neutral (null = calm steady talk)
  return null;
}
