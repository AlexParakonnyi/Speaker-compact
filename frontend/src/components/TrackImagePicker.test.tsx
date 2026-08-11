import { beforeAll, describe, expect, it, vi } from 'vitest'
import { render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'

import { TrackImagePicker } from './TrackImagePicker'
import * as imageLib from '@/lib/image'

vi.mock('@/lib/image', () => ({
  resizeImageToJpeg: vi.fn(),
}))

const mockedResize = vi.mocked(imageLib.resizeImageToJpeg)

// jsdom не умеет ни URL.createObjectURL, ни декодирование картинок.
beforeAll(() => {
  URL.createObjectURL = vi.fn(() => 'blob:mock')
  URL.revokeObjectURL = vi.fn()
})

describe('<TrackImagePicker />', () => {
  it('выбор файла ресайзит его и показывает превью, "Сохранить" вызывает onSave с результатом', async () => {
    const resized = new Blob(['resized'], { type: 'image/jpeg' })
    mockedResize.mockResolvedValue(resized)
    const onSave = vi.fn().mockResolvedValue(undefined)
    const user = userEvent.setup()

    render(
      <TrackImagePicker open trackName="morning.wav" onOpenChange={() => {}} onSave={onSave} />,
    )

    const file = new File(['orig'], 'photo.png', { type: 'image/png' })
    const fileInput = document.querySelector('input[type="file"]:not([capture])') as HTMLInputElement
    await user.upload(fileInput, file)

    await waitFor(() => expect(mockedResize).toHaveBeenCalledWith(file))
    expect(await screen.findByRole('img', { name: /превью/i })).toBeInTheDocument()

    await user.click(screen.getByRole('button', { name: /сохранить/i }))
    await waitFor(() => expect(onSave).toHaveBeenCalledWith(resized))
  })

  it('вставка картинки из буфера обмена (paste) тоже запускает ресайз', async () => {
    const resized = new Blob(['resized'], { type: 'image/jpeg' })
    mockedResize.mockResolvedValue(resized)

    render(<TrackImagePicker open trackName="morning.wav" onOpenChange={() => {}} onSave={vi.fn()} />)

    const file = new File(['orig'], 'clip.png', { type: 'image/png' })
    const pasteEvent = new Event('paste') as ClipboardEvent
    Object.defineProperty(pasteEvent, 'clipboardData', {
      value: { items: [{ type: 'image/png', getAsFile: () => file }] },
    })
    window.dispatchEvent(pasteEvent)

    await waitFor(() => expect(mockedResize).toHaveBeenCalledWith(file))
  })

  it('показывает ошибку, если ресайз не удался', async () => {
    mockedResize.mockRejectedValue(new Error('Canvas 2D недоступен'))
    const user = userEvent.setup()

    render(<TrackImagePicker open trackName="morning.wav" onOpenChange={() => {}} onSave={vi.fn()} />)

    const file = new File(['orig'], 'photo.png', { type: 'image/png' })
    const fileInput = document.querySelector('input[type="file"]:not([capture])') as HTMLInputElement
    await user.upload(fileInput, file)

    expect(await screen.findByText(/canvas 2d недоступен/i)).toBeInTheDocument()
  })
})
