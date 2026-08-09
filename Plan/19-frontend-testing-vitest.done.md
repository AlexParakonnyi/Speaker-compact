# Plan/19 — Frontend Testing with Vitest

**Status:** in-progress  
**Implements:** инфраструктура unit/component тестирования фронтенда на Vitest + React Testing Library.

---

## Цель

Покрыть фронтенд тестами с самого начала, чтобы рефакторинг компонентов при реализации плана (Plan/12-15) не ломал существующее поведение незаметно.

## Инструменты

| Пакет | Роль |
|-------|------|
| `vitest` | Test runner (встроен в Vite, та же конфигурация) |
| `@testing-library/react` | Рендер компонентов в jsdom |
| `@testing-library/user-event` | Симуляция действий пользователя |
| `jsdom` | DOM-окружение в Node (Vitest environment) |
| `@testing-library/jest-dom` | Матчеры типа `toBeInTheDocument()` |

## Файлы

| Файл | Изменения |
|------|-----------|
| `vite.config.ts` | добавить секцию `test: { environment, globals, setupFiles }` |
| `tsconfig.app.json` | добавить `"vitest/globals"` в `compilerOptions.types` |
| `package.json` | добавить скрипты `"test"` и `"test:run"` |
| `src/test/setup.ts` | `import '@testing-library/jest-dom'` |
| `src/lib/utils.test.ts` | тесты функции `cn()` |
| `src/App.test.tsx` | smoke-тест рендера `<App />` |

## Запуск

```bash
npm test          # watch-режим
npm run test:run  # один прогон (CI)
```

## Стиль тестов

- Файлы рядом с тестируемым кодом: `Foo.tsx` → `Foo.test.tsx`
- Общие утилиты: `src/test/setup.ts`, `src/test/utils.ts` (render-хелперы с провайдерами)
- Тест проверяет поведение, а не детали реализации (accessible queries, не CSS-классы)
