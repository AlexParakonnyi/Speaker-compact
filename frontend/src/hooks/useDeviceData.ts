import { useCallback, useEffect, useState } from 'react'

import {
  getGroups,
  getScenarios,
  getStatus,
  type DeviceStatus,
  type GroupsData,
  type ScenariosData,
} from '@/api/device'

// Опрос вместо WebSocket/SSE — устройство и так лёгкое, а несколько клиентов
// (телефон + ноутбук одновременно) должны видеть одно и то же состояние
// (Plan/13: "решить при реализации" — выбран polling ради простоты и как
// единственный источник истины, а не локальный optimistic-стейт).
const POLL_INTERVAL_MS = 2000

export function useDeviceData() {
  const [status, setStatus] = useState<DeviceStatus | null>(null)
  const [groups, setGroups] = useState<GroupsData | null>(null)
  const [scenarios, setScenarios] = useState<ScenariosData | null>(null)
  const [error, setError] = useState<string | null>(null)

  const refetch = useCallback(async () => {
    // allSettled, не all — один упавший запрос (например /api/groups во время
    // шторма stopAP/startAP) не должен прятать уже успешно полученный status.
    const [s, g, sc] = await Promise.allSettled([getStatus(), getGroups(), getScenarios()])
    if (s.status === 'fulfilled') setStatus(s.value)
    if (g.status === 'fulfilled') setGroups(g.value)
    if (sc.status === 'fulfilled') setScenarios(sc.value)

    const failed = [s, g, sc].find((r): r is PromiseRejectedResult => r.status === 'rejected')
    setError(
      failed ? (failed.reason instanceof Error ? failed.reason.message : 'Не удалось связаться с устройством') : null,
    )
  }, [])

  useEffect(() => {
    refetch()
    const id = setInterval(refetch, POLL_INTERVAL_MS)
    return () => clearInterval(id)
  }, [refetch])

  return { status, groups, scenarios, error, refetch }
}
