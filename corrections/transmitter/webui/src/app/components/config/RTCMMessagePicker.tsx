import { useState, useMemo } from 'react';
import { Input } from '../ui/input';
import { Label } from '../ui/label';
import { Checkbox } from '../ui/checkbox';
import { Badge } from '../ui/badge';
import { Search, AlertCircle } from 'lucide-react';
import { RTCM_MESSAGE_TYPES, CATEGORY_COLORS, type RTCMMessageType } from '../../data/rtcm-messages';
import { cn } from '../ui/utils';

interface RTCMMessagePickerProps {
  allowedTypes: number[];
  priorityTypes: number[];
  onChange: (allowed: number[], priority: number[]) => void;
}

export function RTCMMessagePicker({ allowedTypes, priorityTypes, onChange }: RTCMMessagePickerProps) {
  const [search, setSearch] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<RTCMMessageType['category'] | 'All'>('All');

  const categories = useMemo(() => {
    const cats = new Set(RTCM_MESSAGE_TYPES.map(m => m.category));
    return ['All', ...Array.from(cats)] as const;
  }, []);

  const filteredMessages = useMemo(() => {
    let filtered = RTCM_MESSAGE_TYPES;

    if (selectedCategory !== 'All') {
      filtered = filtered.filter(m => m.category === selectedCategory);
    }

    if (search) {
      const searchLower = search.toLowerCase();
      filtered = filtered.filter(m =>
        m.id.toString().includes(search) ||
        m.name.toLowerCase().includes(searchLower) ||
        m.description.toLowerCase().includes(searchLower)
      );
    }

    return filtered;
  }, [search, selectedCategory]);

  const toggleAllowed = (id: number) => {
    const newAllowed = allowedTypes.includes(id)
      ? allowedTypes.filter(t => t !== id)
      : [...allowedTypes, id].sort((a, b) => a - b);

    // If unchecking, also remove from priority
    const newPriority = allowedTypes.includes(id)
      ? priorityTypes.filter(t => t !== id)
      : priorityTypes;

    onChange(newAllowed, newPriority);
  };

  const togglePriority = (id: number) => {
    // Can only be priority if it's allowed
    if (!allowedTypes.includes(id)) return;

    const newPriority = priorityTypes.includes(id)
      ? priorityTypes.filter(t => t !== id)
      : [...priorityTypes, id].sort((a, b) => a - b);

    onChange(allowedTypes, newPriority);
  };

  const groupedMessages = useMemo(() => {
    const grouped = new Map<string, RTCMMessageType[]>();

    filteredMessages.forEach(msg => {
      const category = msg.category;
      if (!grouped.has(category)) {
        grouped.set(category, []);
      }
      grouped.get(category)!.push(msg);
    });

    return Array.from(grouped.entries()).sort(([a], [b]) => a.localeCompare(b));
  }, [filteredMessages]);

  return (
    <div className="space-y-4">
      <div className="space-y-2">
        <Label className="text-sm">RTCM Message Types</Label>
        <div className="relative">
          <Search className="absolute left-2.5 top-1/2 -translate-y-1/2 size-4 text-muted-foreground" />
          <Input
            placeholder="Search by ID or name..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            className="pl-8 h-9 text-sm"
          />
        </div>
      </div>

      <div className="flex gap-1.5 flex-wrap">
        {categories.map(cat => (
          <button
            key={cat}
            onClick={() => setSelectedCategory(cat)}
            className={cn(
              "px-2.5 py-1 rounded-full text-xs font-medium transition-colors whitespace-nowrap",
              selectedCategory === cat
                ? "bg-primary text-primary-foreground"
                : "bg-muted text-muted-foreground hover:bg-muted/80"
            )}
          >
            {cat}
          </button>
        ))}
      </div>

      <div className="border rounded-lg divide-y max-h-[350px] sm:max-h-[450px] overflow-y-auto">
        {groupedMessages.length === 0 ? (
          <div className="p-6 text-center text-muted-foreground text-sm">
            No message types found
          </div>
        ) : (
          groupedMessages.map(([category, messages]) => (
            <div key={category} className="p-2 sm:p-3">
              <div className="flex items-center gap-2 mb-2 px-1">
                <span className="text-xs font-semibold text-muted-foreground uppercase tracking-wide flex-1">
                  {category}
                </span>
                <Badge
                  variant="outline"
                  className={cn("text-xs", CATEGORY_COLORS[category as RTCMMessageType['category']])}
                >
                  {messages.length}
                </Badge>
              </div>
              <div className="space-y-1">
                {messages.map(msg => {
                  const isAllowed = allowedTypes.includes(msg.id);
                  const isPriority = priorityTypes.includes(msg.id);

                  return (
                    <div
                      key={msg.id}
                      className={cn(
                        "flex items-center gap-2 p-2 rounded-md transition-colors",
                        isAllowed && "bg-accent/50"
                      )}
                    >
                      <Checkbox
                        id={`msg-${msg.id}`}
                        checked={isAllowed}
                        onCheckedChange={() => toggleAllowed(msg.id)}
                        className="flex-shrink-0"
                      />

                      <label
                        htmlFor={`msg-${msg.id}`}
                        className="flex-1 min-w-0 cursor-pointer"
                      >
                        <div className="flex items-baseline gap-2">
                          <span className={cn(
                            "font-mono text-xs font-bold flex-shrink-0",
                            isAllowed ? "text-foreground" : "text-muted-foreground"
                          )}>
                            {msg.id}
                          </span>
                          <span className={cn(
                            "text-sm font-medium truncate",
                            isAllowed ? "text-foreground" : "text-muted-foreground"
                          )}>
                            {msg.name}
                          </span>
                        </div>
                        <p className="text-xs text-muted-foreground mt-0.5 line-clamp-1 sm:line-clamp-none">
                          {msg.description}
                        </p>
                      </label>

                      <button
                        onClick={() => togglePriority(msg.id)}
                        disabled={!isAllowed}
                        className={cn(
                          "flex-shrink-0 p-1.5 rounded transition-colors",
                          isAllowed && "hover:bg-accent",
                          !isAllowed && "opacity-30 cursor-not-allowed"
                        )}
                        title={isAllowed ? "Toggle high priority" : "Enable message type first"}
                      >
                        <AlertCircle
                          className={cn(
                            "size-5 transition-colors",
                            isPriority ? "fill-red-500 text-red-500" : "text-muted-foreground"
                          )}
                        />
                      </button>
                    </div>
                  );
                })}
              </div>
            </div>
          ))
        )}
      </div>

      <div className="text-xs text-muted-foreground space-y-1.5 bg-muted/50 p-2.5 rounded-md">
        <div className="flex items-center gap-2">
          <Checkbox checked disabled className="size-3 flex-shrink-0" />
          <span className="leading-tight">Allowed (will be transmitted)</span>
        </div>
        <div className="flex items-center gap-2">
          <AlertCircle className="size-3 fill-red-500 text-red-500 flex-shrink-0" />
          <span className="leading-tight">High priority</span>
        </div>
        <div className="mt-1 pt-1.5 border-t text-foreground">
          <strong>{allowedTypes.length}</strong> allowed · <strong>{priorityTypes.length}</strong> priority
        </div>
      </div>
    </div>
  );
}
