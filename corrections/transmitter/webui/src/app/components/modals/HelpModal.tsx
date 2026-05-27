import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription } from '../ui/dialog';
import type { HelpContent } from '../../data/help-content';

interface HelpModalProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  content: HelpContent;
}

export function HelpModal({ open, onOpenChange, content }: HelpModalProps) {
  // Split content by paragraphs and preserve formatting
  const paragraphs = content.content.split('\n\n');

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-2xl max-h-[80vh] p-0 gap-0 flex flex-col">
        <DialogHeader className="px-6 pt-6 pb-4 flex-shrink-0">
          <DialogTitle>{content.title}</DialogTitle>
          <DialogDescription>Configuration guide and tips</DialogDescription>
        </DialogHeader>

        <div className="overflow-y-auto flex-1 px-6 pb-6">
          <div className="space-y-4 text-sm">
            {paragraphs.map((paragraph, idx) => {
              // Check if it's a bold header (starts with **)
              if (paragraph.trim().startsWith('**') && paragraph.includes('**')) {
                const parts = paragraph.split('**');
                return (
                  <div key={idx}>
                    <h3 className="font-semibold text-foreground mb-1">{parts[1]}</h3>
                    {parts[2] && <p className="text-muted-foreground">{parts[2].trim()}</p>}
                  </div>
                );
              }

              // Check if it's a tip (starts with **Tip:**)
              if (paragraph.trim().startsWith('**Tip:**')) {
                const text = paragraph.replace('**Tip:**', '').trim();
                return (
                  <div key={idx} className="bg-blue-50 dark:bg-blue-950/20 border border-blue-200 dark:border-blue-800 rounded-lg p-3">
                    <p className="text-blue-900 dark:text-blue-100">
                      <span className="font-semibold">💡 Tip:</span> {text}
                    </p>
                  </div>
                );
              }

              // Check if it's a bullet list
              if (paragraph.trim().startsWith('•')) {
                const items = paragraph.split('\n').filter(line => line.trim().startsWith('•'));
                return (
                  <ul key={idx} className="space-y-2 ml-4">
                    {items.map((item, itemIdx) => (
                      <li key={itemIdx} className="text-muted-foreground flex gap-2">
                        <span className="text-primary flex-shrink-0">•</span>
                        <span>{item.replace('•', '').trim()}</span>
                      </li>
                    ))}
                  </ul>
                );
              }

              // Regular paragraph
              return (
                <p key={idx} className="text-muted-foreground leading-relaxed">
                  {paragraph}
                </p>
              );
            })}
          </div>
        </div>
      </DialogContent>
    </Dialog>
  );
}
