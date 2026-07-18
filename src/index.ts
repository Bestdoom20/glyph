// GLYPH — living kaomoji face. P0 stub: static face render.
export function mount(el: HTMLElement) {
  const face = document.createElement('div');
  face.textContent = '・_・';
  face.style.font = "700 64px/1 ui-monospace, 'Cascadia Mono', monospace";
  el.appendChild(face);
  return face;
}
