// Plan/13 п.2: заглушка без фото — не пустой квадрат, акцентный цвет по хэшу
// имени файла, чтобы плитки без фото не выглядели одинаково-безликими.
export function hashColor(input: string): string {
  let hash = 0
  for (let i = 0; i < input.length; i++) {
    hash = (hash << 5) - hash + input.charCodeAt(i)
    hash |= 0
  }
  const hue = Math.abs(hash) % 360
  return `hsl(${hue}, 55%, 45%)`
}
