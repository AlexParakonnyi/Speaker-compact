import { useState } from 'react'

import { Button } from '@/components/ui/button'
import { ScenariosScreen } from '@/screens/ScenariosScreen'
import { TracksScreen } from '@/screens/TracksScreen'

type Tab = 'tracks' | 'scenarios'

// Без react-router — два экрана, простого переключателя таба достаточно
// (тот же принцип минимализма: не тащить зависимость раньше, чем она
// реально нужна; появится больше экранов — заведём router).
function App() {
  const [tab, setTab] = useState<Tab>('tracks')

  return (
    <div className="min-h-svh">
      <nav className="sticky top-0 z-10 flex gap-2 border-b bg-background p-2">
        <Button variant={tab === 'tracks' ? 'default' : 'ghost'} size="sm" onClick={() => setTab('tracks')}>
          Треки
        </Button>
        <Button variant={tab === 'scenarios' ? 'default' : 'ghost'} size="sm" onClick={() => setTab('scenarios')}>
          Сценарии
        </Button>
      </nav>
      {tab === 'tracks' ? <TracksScreen /> : <ScenariosScreen />}
    </div>
  )
}

export default App
