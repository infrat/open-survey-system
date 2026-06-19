# AGENTS.md

## Project Context

This is a mobile-first PWA for configuring GNSS RTK transmitters running on ESP32. The app works offline without internet connectivity.

## Code Style Guidelines

### Always Follow

1. **TypeScript strict mode** - All files must be `.tsx` or `.ts`
2. **Mobile-first responsive** - Start with mobile layout, add `sm:` breakpoints for desktop
3. **Tailwind-only styling** - No inline styles, no CSS modules
4. **Controlled components** - All form inputs must have `value` and `onChange`
5. **Named exports** - Prefer named exports over default exports

### Never Do

- Don't create `.js` or `.jsx` files
- Don't use class components
- Don't add font-size or font-weight Tailwind classes (use theme.css tokens)
- Don't import from CDN (everything must be bundled)
- Don't use external APIs without user's explicit instruction

## File Naming Conventions

- Components: PascalCase (e.g., `WiFiConfig.tsx`)
- Hooks: camelCase with "use" prefix (e.g., `useConfig.ts`)
- Utils/data: camelCase (e.g., `rtcm-messages.ts`)
- Types: match the domain (e.g., `config.ts` for Config type)

## Component Template

```typescript
import { SomeUIComponent } from '../ui/some-component';
import type { Config } from '../../types/config';

interface MyComponentProps {
  value: string;
  onChange: (value: string) => void;
}

export function MyComponent({ value, onChange }: MyComponentProps) {
  return (
    <div className="space-y-4">
      {/* Mobile-first layout */}
    </div>
  );
}
```

## Common Patterns

### Form Input Pattern

```typescript
<div className="space-y-2">
  <Label htmlFor="field-id">Field Name *</Label>
  <Input
    id="field-id"
    type="text"
    value={config.field}
    onChange={(e) => onChange({ ...config, field: e.target.value })}
    required
  />
  <p className="text-xs text-muted-foreground">Helper text</p>
</div>
```

### API Call Pattern (in useConfig hook)

```typescript
const apiMethod = async (): Promise<ReturnType> => {
  setLoading(true);
  setError(null);
  try {
    const response = await fetch(`${API_BASE}/endpoint`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data),
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.message);
    return result;
  } catch (err) {
    setError(err instanceof Error ? err.message : "Unknown error");
    return fallbackValue;
  } finally {
    setLoading(false);
  }
};
```

### Modal Pattern

```typescript
<Dialog open={showModal} onOpenChange={setShowModal}>
  <DialogContent className="max-w-md max-h-[80vh] flex flex-col">
    <DialogHeader>
      <DialogTitle>Title</DialogTitle>
      <DialogDescription>Description</DialogDescription>
    </DialogHeader>
    <div className="overflow-y-auto flex-1">
      {/* Scrollable content */}
    </div>
  </DialogContent>
</Dialog>
```

### Conditional Styling Pattern

```typescript
import { cn } from '../ui/utils';

<div className={cn(
  "base-classes",
  condition && "conditional-classes",
  variant === 'primary' && "variant-classes"
)} />
```

## State Management Rules

1. Configuration state lives in `useConfig` hook
2. UI-only state (modals, loading) stays in component
3. Pass down only the section of config needed: `config.wifi` not entire `config`
4. Update pattern: `onChange({ ...config, field: newValue })`

## Validation Rules

- Validation happens on form submit in `App.tsx` `handleSave()`
- Show errors with `toast.error()` from `sonner`
- Required fields marked with `*` in label
- No inline validation (except for HTML5 attributes like `min`/`max`)

## Responsive Breakpoints

- Mobile: default (no prefix) - 320px+
- Tablet: `sm:` - 640px+
- Desktop: `md:` - 768px+
- Large: `lg:` - 1024px+

Design for mobile first, then add larger breakpoints as needed.

## Accessibility Requirements

- All inputs must have associated `<Label>` with matching `htmlFor`
- Buttons must have text or `aria-label`
- Modals must have `DialogTitle` and `DialogDescription`
- Interactive elements minimum 44px touch target on mobile
- Color contrast must meet WCAG AA standards

## Icon Usage

- Use `lucide-react` for all icons
- Import: `import { IconName } from 'lucide-react'`
- Size: `className="size-4"` for inline, `size-5` for buttons
- Common icons: Save, RefreshCw, Power, HelpCircle, AlertCircle, MapPin

## Error Handling

- API errors: catch, set error state, show toast
- Form validation: check before submit, show toast with specific message
- Graceful degradation: feature detection for geolocation, PWA features
- Never crash - always provide fallback

## Performance Considerations

- Use `useMemo` for expensive computations (filtering, sorting large lists)
- Use `useState` for UI state, not derived values
- Avoid unnecessary re-renders - pass stable references
- Images: prefer SVG for logos/icons, PNG for photos

## Testing Checklist (for new features)

- [ ] Works on mobile (375px width - iPhone SE)
- [ ] Works on tablet (768px width - iPad)
- [ ] All required fields validated
- [ ] Error states handled gracefully
- [ ] Loading states shown during async operations
- [ ] Keyboard navigation works
- [ ] Touch targets minimum 44px
- [ ] Help text provided where needed

## When in Doubt

- Check existing components in `src/app/components/config/` for patterns
- Read `docs/development.md` for architectural decisions
- Look at `src/app/App.tsx` for state management examples
- Refer to `src/imports/config-structure.md` for API contract details
