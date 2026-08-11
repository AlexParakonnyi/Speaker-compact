// Plan/06: обязательный ресайз перед отправкой — телефонные фото по 3-10МБ,
// устройство (SD + HTTP-стек ESP32) не должно с этим работать. Целевой
// размер по длинной стороне и JPEG-качество — из плана.
const MAX_DIMENSION = 600
const JPEG_QUALITY = 0.8

export async function resizeImageToJpeg(source: Blob): Promise<Blob> {
  const bitmap = await createImageBitmap(source)
  const scale = Math.min(1, MAX_DIMENSION / Math.max(bitmap.width, bitmap.height))
  const width = Math.round(bitmap.width * scale)
  const height = Math.round(bitmap.height * scale)

  const canvas = document.createElement('canvas')
  canvas.width = width
  canvas.height = height
  const ctx = canvas.getContext('2d')
  if (!ctx) throw new Error('Canvas 2D недоступен')
  ctx.drawImage(bitmap, 0, 0, width, height)
  bitmap.close()

  const blob = await new Promise<Blob | null>((resolve) =>
    canvas.toBlob(resolve, 'image/jpeg', JPEG_QUALITY),
  )
  if (!blob) throw new Error('Не удалось закодировать JPEG')
  return blob
}
