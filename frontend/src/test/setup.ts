import { vi } from 'vitest'

import '@testing-library/jest-dom'

// framer-motion в тестах — синхронный no-op passthrough, не настоящие
// анимации. Без этого AnimatePresence держит "исчезающие" элементы в DOM
// во время exit-transition, что ломает синхронные проверки вида
// not.toBeInTheDocument() сразу после действия (см. Plan/12). Reorder —
// тоже мокается: реальный drag через framer-motion не воспроизвести в
// jsdom (нет реальных pointer-move дельт), поэтому в тестах проверяем
// только рендер/данные, не сам жест перетаскивания.
vi.mock('framer-motion', async () => {
  const React = await import('react')

  const stripMotionProps = (props: Record<string, unknown>) => {
    const {
      initial: _initial,
      animate: _animate,
      exit: _exit,
      transition: _transition,
      layout: _layout,
      layoutId: _layoutId,
      whileDrag: _whileDrag,
      whileHover: _whileHover,
      whileTap: _whileTap,
      variants: _variants,
      ...rest
    } = props
    return rest
  }

  const motion = new Proxy(
    {},
    {
      get: (_target, tag: string) =>
        React.forwardRef((props: Record<string, unknown>, ref: React.Ref<unknown>) => {
          const { children, ...rest } = props
          return React.createElement(tag, { ...stripMotionProps(rest), ref }, children as React.ReactNode)
        }),
    },
  )

  const AnimatePresence = ({ children }: { children?: React.ReactNode }) => children

  const asTag = (props: Record<string, unknown>) => {
    const { as, ...domProps } = props
    return { Tag: (as as string) ?? 'div', domProps }
  }

  const ReorderGroup = React.forwardRef((props: Record<string, unknown>, ref: React.Ref<unknown>) => {
    const { children, values: _values, onReorder: _onReorder, axis: _axis, ...rest } = props
    const { Tag, domProps } = asTag(rest)
    return React.createElement(Tag, { ...domProps, ref }, children as React.ReactNode)
  })

  const ReorderItem = React.forwardRef((props: Record<string, unknown>, ref: React.Ref<unknown>) => {
    const { children, value: _value, dragListener: _dragListener, dragControls: _dragControls, ...rest } = props
    const { Tag, domProps } = asTag(rest)
    return React.createElement(Tag, { ...domProps, ref }, children as React.ReactNode)
  })

  const useDragControls = () => ({ start: () => {} })

  return {
    motion,
    AnimatePresence,
    Reorder: { Group: ReorderGroup, Item: ReorderItem },
    useDragControls,
  }
})

// jsdom не реализует ResizeObserver — нужен Radix Slider (@radix-ui/react-use-size)
// и, вероятно, vaul Drawer. Не тестируем реальные размеры/ресайз, просто заглушка,
// чтобы компоненты не падали при монтировании.
if (typeof globalThis.ResizeObserver === 'undefined') {
  globalThis.ResizeObserver = class ResizeObserver {
    observe() {}
    unobserve() {}
    disconnect() {}
  }
}

// jsdom тоже не реализует Pointer Capture API — нужен vaul (Drawer) для
// drag-жестов bottom sheet. Без этого необработанные исключения из
// onPointerDown бьют по соседним тестам (async, всплывают после теста).
if (!Element.prototype.hasPointerCapture) {
  Element.prototype.hasPointerCapture = () => false
}
if (!Element.prototype.setPointerCapture) {
  Element.prototype.setPointerCapture = () => {}
}
if (!Element.prototype.releasePointerCapture) {
  Element.prototype.releasePointerCapture = () => {}
}
