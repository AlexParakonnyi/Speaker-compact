import { describe, it, expect } from 'vitest'
import { render, screen } from '@testing-library/react'
import App from './App'

describe('<App />', () => {
  it('отображает заголовок Speaker-compact', () => {
    render(<App />)
    expect(screen.getByRole('heading', { name: /speaker-compact/i })).toBeInTheDocument()
  })

  it('содержит информацию о планах реализации', () => {
    render(<App />)
    expect(screen.getByText(/plan\/12/i)).toBeInTheDocument()
  })
})
