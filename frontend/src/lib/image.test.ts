import { afterEach, describe, expect, it, vi } from 'vitest'

import { resizeImageToJpeg } from './image'

function mockBitmap(width: number, height: number) {
  return { width, height, close: vi.fn() }
}

function stubCanvas(ctx: unknown, blob: Blob | null) {
  vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockReturnValue(ctx as CanvasRenderingContext2D)
  vi.spyOn(HTMLCanvasElement.prototype, 'toBlob').mockImplementation((cb) => cb(blob))
}

describe('resizeImageToJpeg', () => {
  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('уменьшает изображение крупнее 600px по длинной стороне, сохраняя пропорции', async () => {
    vi.stubGlobal('createImageBitmap', vi.fn().mockResolvedValue(mockBitmap(1200, 600)))
    const drawImage = vi.fn()
    const outBlob = new Blob(['jpeg'], { type: 'image/jpeg' })
    stubCanvas({ drawImage }, outBlob)

    const result = await resizeImageToJpeg(new Blob(['orig']))

    expect(drawImage).toHaveBeenCalledWith(expect.anything(), 0, 0, 600, 300)
    expect(result).toBe(outBlob)
    vi.unstubAllGlobals()
  })

  it('не увеличивает изображение, которое уже меньше лимита', async () => {
    vi.stubGlobal('createImageBitmap', vi.fn().mockResolvedValue(mockBitmap(300, 200)))
    const drawImage = vi.fn()
    stubCanvas({ drawImage }, new Blob(['jpeg']))

    await resizeImageToJpeg(new Blob(['orig']))

    expect(drawImage).toHaveBeenCalledWith(expect.anything(), 0, 0, 300, 200)
    vi.unstubAllGlobals()
  })

  it('бросает ошибку, если 2D-контекст недоступен', async () => {
    vi.stubGlobal('createImageBitmap', vi.fn().mockResolvedValue(mockBitmap(100, 100)))
    stubCanvas(null, null)

    await expect(resizeImageToJpeg(new Blob(['orig']))).rejects.toThrow('Canvas 2D')
    vi.unstubAllGlobals()
  })

  it('бросает ошибку, если toBlob не смог закодировать JPEG', async () => {
    vi.stubGlobal('createImageBitmap', vi.fn().mockResolvedValue(mockBitmap(100, 100)))
    stubCanvas({ drawImage: vi.fn() }, null)

    await expect(resizeImageToJpeg(new Blob(['orig']))).rejects.toThrow('JPEG')
    vi.unstubAllGlobals()
  })
})
