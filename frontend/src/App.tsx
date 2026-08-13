import { useState } from 'react'
import { motion } from 'framer-motion'

import { Button } from '@/components/ui/button'
import { BatteryIndicator } from '@/components/BatteryIndicator'
import { useDeviceData } from '@/hooks/useDeviceData'
import { ScenariosScreen } from '@/screens/ScenariosScreen'
import { TracksScreen } from '@/screens/TracksScreen'

type Tab = 'tracks' | 'scenarios'

// Без react-router — два экрана, простого переключателя таба достаточно
// (тот же принцип минимализма: не тащить зависимость раньше, чем она
// реально нужна; появится больше экранов — заведём router).
function App() {
  const [tab, setTab] = useState<Tab>('tracks')
  // Отдельный опрос только ради индикатора батареи в шапке (дублирует
  // поллинг, который уже делают экраны) — осознанный компромисс: пробрасывать
  // status пропсами через оба экрана сейчас не стоит того, устройство
  // локальное, лишний GET раз в 2с не заметен.
  const { status } = useDeviceData()

  return (
    <div className="min-h-svh">
      <nav className="sticky top-0 z-10 flex items-center justify-between gap-2 border-b bg-background p-2">
        <div className="flex gap-2">
          <Button variant={tab === 'tracks' ? 'default' : 'ghost'} size="sm" onClick={() => setTab('tracks')}>
            Треки
          </Button>
          <Button variant={tab === 'scenarios' ? 'default' : 'ghost'} size="sm" onClick={() => setTab('scenarios')}>
            Сценарии
          </Button>
        </div>
        {status && (
          <BatteryIndicator valid={status.batteryValid} voltage={status.batteryVoltage} percent={status.batteryPercent} />
        )}
      </nav>
      <motion.div key={tab} initial={{ opacity: 0, y: 6 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.15 }}>
        {tab === 'tracks' ? <TracksScreen /> : <ScenariosScreen />}
      </motion.div>
    </div>
  )
}

export default App
