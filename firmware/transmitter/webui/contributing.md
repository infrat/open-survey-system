# Contributing to OSS RTCM Transmitter

## Quick Start

```bash
# Install dependencies
pnpm install

# Start dev server (preview only - PWA won't work)
pnpm dev

# Build for production (ESP32)
pnpm run build:esp32

# Generate PWA icons (after updating logo)
pnpm run generate:icons
```

## Before Committing

1. **TypeScript check**: Ensure no type errors
2. **Test on mobile viewport**: Minimum 375px width (iPhone SE)
3. **Check responsiveness**: Test sm: and md: breakpoints
4. **Validate forms**: Test all required field validation
5. **API integration**: Test against real ESP32 or mock responses

## Pull Request Guidelines

### PR Title Format

```
feat: Add new configuration section
fix: Resolve mobile layout issue in RTCM picker
docs: Update API integration guide
style: Improve spacing in WiFi config form
```

### PR Description Template

```markdown
## What

Brief description of changes

## Why

Why this change is needed

## Testing

- [ ] Tested on mobile (375px)
- [ ] Tested on tablet (768px)
- [ ] Form validation works
- [ ] No TypeScript errors
- [ ] PWA icons regenerated (if logo changed)

## Screenshots

(if UI changes)
```

## Code Review Checklist

### Functionality

- [ ] Feature works as intended
- [ ] Edge cases handled
- [ ] Error states handled gracefully
- [ ] Loading states shown during async operations

### Code Quality

- [ ] No TypeScript errors or `any` types
- [ ] Components follow existing patterns
- [ ] No duplicate code (extract to shared component/util)
- [ ] Proper prop types defined

### UI/UX

- [ ] Mobile-first responsive design
- [ ] Touch targets minimum 44px
- [ ] Accessible (labels, ARIA attributes)
- [ ] Consistent with existing design
- [ ] Help text provided for complex features

### Performance

- [ ] No unnecessary re-renders
- [ ] Expensive computations memoized
- [ ] Images optimized (SVG preferred)
- [ ] Bundle size considered

## Common Tasks

### Adding a New Config Section

See `development.md` → "Adding New Features" → "Adding a New Configuration Tab"

### Updating UI Components

```bash
# Add new shadcn/ui component
npx shadcn@latest add <component-name>

# Customize in src/app/components/ui/
# Follow existing component patterns
```

### Updating Help Content

Edit `src/app/data/help-content.ts`:

- Keep formatting simple (markdown-like)
- Use `**Bold**` for field names
- Add `**Tip:**` for helpful hints
- Separate paragraphs with blank lines

### Updating RTCM Message Types

Edit `src/app/data/rtcm-messages.ts`:

```typescript
{
  id: 1234,
  name: 'Message Name',
  description: 'What this message does',
  category: 'GPS' | 'GLONASS' | 'Galileo' | 'BeiDou' | 'Multi-GNSS' | 'Station' | 'Other'
}
```

### Debugging API Issues

1. Check browser DevTools Network tab
2. Verify request payload matches expected format
3. Check ESP32 serial console for backend errors
4. Test with mock data in `useConfig.ts` first

## Styling Guidelines

### Spacing Scale

- `space-y-1` / `gap-1` = 0.25rem (4px) - tight
- `space-y-2` / `gap-2` = 0.5rem (8px) - default
- `space-y-4` / `gap-4` = 1rem (16px) - sections
- `space-y-6` / `gap-6` = 1.5rem (24px) - major sections

### Colors

Use semantic tokens from theme, not hardcoded colors:

- `text-foreground` - primary text
- `text-muted-foreground` - secondary text
- `bg-background` - main background
- `bg-accent` - highlighted background
- `border` - borders
- `text-primary` - brand color
- `text-destructive` - errors

### Typography

Don't use Tailwind text size classes - use theme.css tokens:

- Headers: Already styled in theme.css
- Body: Default size
- Helper text: `text-xs text-muted-foreground`
- Labels: Use `<Label>` component

## Build & Deploy

### Local Testing

```bash
# Build
pnpm run build:esp32

# Serve locally
cd dist-esp32
python3 -m http.server 8080

# Test PWA
# Open http://localhost:8080 in Chrome
# DevTools → Application → Manifest / Service Workers
```

### ESP32 Deployment

1. Build: `pnpm run build:esp32`
2. Contents of `dist-esp32/` → ESP32 SPIFFS/LittleFS
3. Serve at root: `/index.html`
4. API endpoints at `/api/*`

### PWA Icon Updates

```bash
# 1. Update public/oss-logo.svg
# 2. Regenerate icons
pnpm run generate:icons

# 3. Rebuild
pnpm run build:esp32
```

## Troubleshooting

### "Module not found" errors

- Check import path (relative vs absolute)
- Verify file extension (.tsx not .ts for React components)
- Run `pnpm install` to ensure dependencies installed

### Tailwind classes not applying

- Check for typos in class names
- Verify class exists in Tailwind v4
- Don't use font-_/text-_ size classes (use theme tokens)

### Form not submitting

- Check validation logic in `App.tsx` `handleSave()`
- Verify all required fields have values
- Check browser console for errors

### PWA not working

- Build for production first: `pnpm run build:esp32`
- PWA only works in production build, not dev server
- Check HTTPS or localhost (required for service worker)

### Geolocation not working

- Only works in secure context (HTTPS or localhost)
- Works better in installed PWA
- Fallback: manual input always available

## Resources

- **Project Docs**: `development.md`
- **Copilot Guide**: `.github/copilot-instructions.md`
- **API Spec**: `src/imports/config-structure.md`
- **Component Library**: [Radix UI](https://www.radix-ui.com/primitives)
- **Styling**: [Tailwind CSS v4](https://tailwindcss.com/docs)
- **Icons**: [Lucide React](https://lucide.dev)

## Getting Help

1. Check existing components for similar patterns
2. Read `development.md` for architectural decisions
3. Review `.github/copilot-instructions.md` for code guidelines
4. Test against real ESP32 device when possible
5. Use GitHub Copilot for code suggestions (configured for this project)

## Code of Conduct

- Write clear, self-documenting code
- Comment only when "why" is non-obvious
- Keep PRs focused and atomic
- Test on real devices when possible
- Mobile-first, always
