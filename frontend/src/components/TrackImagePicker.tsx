import { useEffect, useRef, useState } from 'react'
import { Camera, Clipboard, Image as ImageIcon } from 'lucide-react'

import { Button } from '@/components/ui/button'
import {
  Drawer,
  DrawerContent,
  DrawerDescription,
  DrawerFooter,
  DrawerHeader,
  DrawerTitle,
} from '@/components/ui/drawer'
import { resizeImageToJpeg } from '@/lib/image'

interface TrackImagePickerProps {
  open: boolean
  onOpenChange: (open: boolean) => void
  trackName: string
  onSave: (image: Blob) => Promise<void> | void
}

// Plan/06: три источника (буфер обмена / камера / файл), обязательный
// ресайз на канвасе перед показом превью — не отправляем оригинал.
export function TrackImagePicker({ open, onOpenChange, trackName, onSave }: TrackImagePickerProps) {
  const [previewBlob, setPreviewBlob] = useState<Blob | null>(null)
  const [previewUrl, setPreviewUrl] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [saving, setSaving] = useState(false)
  const cameraInputRef = useRef<HTMLInputElement>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)

  useEffect(() => {
    return () => {
      if (previewUrl) URL.revokeObjectURL(previewUrl)
    }
  }, [previewUrl])

  useEffect(() => {
    if (!open) return
    const handlePaste = (e: ClipboardEvent) => {
      const item = Array.from(e.clipboardData?.items ?? []).find((i) => i.type.startsWith('image/'))
      const file = item?.getAsFile()
      if (file) void handleSource(file)
    }
    window.addEventListener('paste', handlePaste)
    return () => window.removeEventListener('paste', handlePaste)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [open])

  const handleSource = async (source: Blob) => {
    setError(null)
    try {
      const resized = await resizeImageToJpeg(source)
      setPreviewBlob(resized)
      setPreviewUrl((old) => {
        if (old) URL.revokeObjectURL(old)
        return URL.createObjectURL(resized)
      })
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Не удалось обработать изображение')
    }
  }

  const reset = () => {
    setPreviewBlob(null)
    setPreviewUrl((old) => {
      if (old) URL.revokeObjectURL(old)
      return null
    })
    setError(null)
  }

  const handleSave = async () => {
    if (!previewBlob) return
    setSaving(true)
    try {
      await onSave(previewBlob)
      reset()
      onOpenChange(false)
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Не удалось сохранить')
    } finally {
      setSaving(false)
    }
  }

  return (
    <Drawer
      open={open}
      onOpenChange={(next) => {
        if (!next) reset()
        onOpenChange(next)
      }}
    >
      <DrawerContent className="p-4">
        <DrawerHeader className="px-0">
          <DrawerTitle>Фото трека</DrawerTitle>
          <DrawerDescription>{trackName}</DrawerDescription>
        </DrawerHeader>

        {previewUrl ? (
          <img src={previewUrl} alt="Превью" className="mx-auto max-h-64 rounded-md object-contain" />
        ) : (
          <div className="flex flex-wrap gap-2">
            <Button type="button" variant="outline" onClick={() => cameraInputRef.current?.click()}>
              <Camera /> Камера
            </Button>
            <Button type="button" variant="outline" onClick={() => fileInputRef.current?.click()}>
              <ImageIcon /> Файл
            </Button>
            <Button
              type="button"
              variant="outline"
              onClick={async () => {
                try {
                  const items = await navigator.clipboard.read()
                  const item = items.find((i) => i.types.some((t) => t.startsWith('image/')))
                  const type = item?.types.find((t) => t.startsWith('image/'))
                  if (item && type) await handleSource(await item.getType(type))
                  else setError('В буфере обмена нет картинки')
                } catch {
                  setError('Нет доступа к буферу обмена — попробуйте Ctrl+V прямо здесь')
                }
              }}
            >
              <Clipboard /> Буфер обмена
            </Button>
          </div>
        )}

        <input
          ref={cameraInputRef}
          type="file"
          accept="image/*"
          capture="environment"
          className="hidden"
          onChange={(e) => {
            const file = e.target.files?.[0]
            e.target.value = ''
            if (file) void handleSource(file)
          }}
        />
        <input
          ref={fileInputRef}
          type="file"
          accept="image/*"
          className="hidden"
          onChange={(e) => {
            const file = e.target.files?.[0]
            e.target.value = ''
            if (file) void handleSource(file)
          }}
        />

        {error && <p className="text-sm text-destructive">{error}</p>}

        <DrawerFooter className="px-0">
          {previewUrl ? (
            <>
              <Button variant="outline" onClick={reset} disabled={saving}>
                Выбрать другое
              </Button>
              <Button onClick={handleSave} disabled={saving}>
                {saving ? 'Сохранение…' : 'Сохранить'}
              </Button>
            </>
          ) : (
            <Button variant="outline" onClick={() => onOpenChange(false)}>
              Отмена
            </Button>
          )}
        </DrawerFooter>
      </DrawerContent>
    </Drawer>
  )
}
