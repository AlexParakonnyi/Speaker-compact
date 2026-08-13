import { describe, expect, it } from 'vitest'
import { render, screen } from '@testing-library/react'

import { BatteryIndicator } from './BatteryIndicator'

describe('<BatteryIndicator />', () => {
  it('показывает и процент, и напряжение, когда датчик отвечает', () => {
    render(<BatteryIndicator valid voltage={3.8} percent={62} />)
    expect(screen.getByText(/62%/)).toBeInTheDocument()
    expect(screen.getByText(/3\.80В/)).toBeInTheDocument()
  })

  it('не показывает процент/напряжение, если датчик не отвечает (batteryValid=false)', () => {
    render(<BatteryIndicator valid={false} voltage={0} percent={0} />)
    expect(screen.queryByText(/%/)).not.toBeInTheDocument()
    expect(screen.queryByText(/В/)).not.toBeInTheDocument()
  })

  it('подсказка (title) предупреждает, что процент условный до реальной батареи', () => {
    render(<BatteryIndicator valid voltage={3.8} percent={62} />)
    const title = screen.getByText(/62%/).closest('span')?.getAttribute('title')
    expect(title).toContain('процент условный')
  })
})
