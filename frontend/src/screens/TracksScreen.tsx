import { useMemo, useRef, useState } from 'react'
import { AnimatePresence, motion } from 'framer-motion'
import { Settings2, Trash2, Upload } from 'lucide-react'

import {
  assignGroup,
  clearAllTracks,
  createGroup,
  deleteGroup,
  deleteTrack,
  playTrack,
  renameGroup,
  renameTrack,
  stopPlayback,
  uploadTrack,
  uploadTrackImage,
  AUDIO_EXTENSIONS,
  MAX_UPLOAD_BYTES,
} from '@/api/device'
import { Button } from '@/components/ui/button'
import { Skeleton } from '@/components/ui/skeleton'
import { ConfirmDialog } from '@/components/ConfirmDialog'
import { GroupFilterChips } from '@/components/GroupFilterChips'
import { GroupsManager } from '@/components/GroupsManager'
import { TrackCard } from '@/components/TrackCard'
import { useDeviceData } from '@/hooks/useDeviceData'

export function TracksScreen() {
  const { status, groups, error, refetch } = useDeviceData()
  const [filter, setFilter] = useState<string | null>(null)
  const [actionError, setActionError] = useState<string | null>(null)
  const [showGroupsManager, setShowGroupsManager] = useState(false)
  const [confirmingClearAll, setConfirmingClearAll] = useState(false)
  const [uploadPct, setUploadPct] = useState<number | null>(null)
  const fileInputRef = useRef<HTMLInputElement>(null)

  const assignments = groups?.assignments ?? {}
  const groupList = groups?.groups ?? []
  const tracks = status?.tracks ?? []

  const counts = useMemo(() => {
    const c: Record<string, number> = { '' : 0 }
    for (const t of tracks) {
      const g = assignments[t.name] ?? ''
      c[g] = (c[g] ?? 0) + 1
    }
    return c
  }, [tracks, assignments])

  const filteredTracks = useMemo(() => {
    if (filter === null) return tracks
    return tracks.filter((t) => (assignments[t.name] ?? '') === filter)
  }, [tracks, assignments, filter])

  const runAction = async (fn: () => Promise<void>) => {
    try {
      setActionError(null)
      await fn()
      await refetch()
    } catch (e) {
      setActionError(e instanceof Error ? e.message : 'Не удалось выполнить действие')
    }
  }

  const handleUpload = async (file: File) => {
    const ext = file.name.slice(file.name.lastIndexOf('.')).toLowerCase()
    if (!AUDIO_EXTENSIONS.includes(ext)) {
      setActionError(`Неподдерживаемое расширение (нужно: ${AUDIO_EXTENSIONS.join(', ')})`)
      return
    }
    if (file.size > MAX_UPLOAD_BYTES) {
      setActionError('Файл больше 20 МБ — устройство откажет')
      return
    }
    setActionError(null)
    setUploadPct(0)
    try {
      await uploadTrack(file, setUploadPct)
      await refetch()
    } catch (e) {
      setActionError(e instanceof Error ? e.message : 'Загрузка не удалась')
    } finally {
      setUploadPct(null)
    }
  }

  if (error && !status) {
    return (
      <div className="flex min-h-svh flex-col items-center justify-center gap-3 p-4 text-center">
        <p className="text-sm text-destructive">{error}</p>
        <Button onClick={refetch}>Повторить</Button>
      </div>
    )
  }

  if (!status || !groups) {
    return (
      <div className="grid grid-cols-2 gap-3 p-4">
        {Array.from({ length: 4 }).map((_, i) => (
          <Skeleton key={i} className="h-40 w-full" />
        ))}
      </div>
    )
  }

  return (
    <div className="mx-auto flex max-w-2xl flex-col gap-3 p-4 pb-24">
      <header className="flex items-center justify-between">
        <h1 className="text-lg font-semibold">Треки</h1>
        {!status.sdMounted && <span className="text-sm text-destructive">SD не смонтирована</span>}
      </header>

      {(error || actionError) && (
        <p className="rounded-md bg-destructive/10 p-2 text-sm text-destructive">{actionError ?? error}</p>
      )}

      <GroupFilterChips groups={groupList} counts={counts} selected={filter} onSelect={setFilter} />

      <div className="flex flex-wrap items-center gap-2">
        <input
          ref={fileInputRef}
          type="file"
          accept="audio/*"
          className="hidden"
          onChange={(e) => {
            const file = e.target.files?.[0]
            e.target.value = ''
            if (file) handleUpload(file)
          }}
        />
        <Button size="sm" onClick={() => fileInputRef.current?.click()} disabled={uploadPct !== null}>
          <Upload /> {uploadPct !== null ? `Загрузка ${uploadPct}%` : 'Загрузить трек'}
        </Button>
        <Button size="sm" variant="outline" onClick={() => setShowGroupsManager((v) => !v)}>
          <Settings2 /> Группы
        </Button>
        <Button
          size="sm"
          variant="outline"
          className="text-destructive"
          onClick={() => setConfirmingClearAll(true)}
          disabled={tracks.length === 0}
        >
          <Trash2 /> Очистить всё
        </Button>
      </div>

      {showGroupsManager && (
        <GroupsManager
          groups={groupList}
          onCreate={(name) => runAction(() => createGroup(name))}
          onRename={(from, to) => runAction(() => renameGroup(from, to))}
          onDelete={(name) => runAction(() => deleteGroup(name))}
        />
      )}

      {filteredTracks.length === 0 ? (
        <p className="py-8 text-center text-sm text-muted-foreground">
          {tracks.length === 0 ? 'Треков пока нет — загрузите первый.' : 'Нет треков в этой группе.'}
        </p>
      ) : (
        <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
          <AnimatePresence initial={false}>
            {filteredTracks.map((t) => (
              <motion.div
                key={t.name}
                layout
                initial={{ opacity: 0, scale: 0.96 }}
                animate={{ opacity: 1, scale: 1 }}
                exit={{ opacity: 0, scale: 0.96 }}
                transition={{ duration: 0.18 }}
              >
                <TrackCard
                  track={t}
                  group={assignments[t.name] ?? ''}
                  groups={groupList}
                  isPlaying={status.status === 'PLAYING' && status.currentTrack === t.name}
                  isBusy={status.status === 'PLAYING' && status.currentTrack !== t.name}
                  onPlay={() => runAction(() => playTrack(t.name))}
                  onStop={() => runAction(() => stopPlayback())}
                  onDelete={() => runAction(() => deleteTrack(t.name))}
                  onRename={(newName) => runAction(() => renameTrack(t.name, newName))}
                  onAssignGroup={(group) => runAction(() => assignGroup(t.name, group))}
                  onImageSave={(image) => uploadTrackImage(t.name, image)}
                />
              </motion.div>
            ))}
          </AnimatePresence>
        </div>
      )}

      <ConfirmDialog
        open={confirmingClearAll}
        onOpenChange={setConfirmingClearAll}
        title="Удалить все треки?"
        description="Все аудиофайлы на устройстве будут удалены безвозвратно. Группы останутся."
        onConfirm={() => runAction(() => clearAllTracks())}
      />
    </div>
  )
}
