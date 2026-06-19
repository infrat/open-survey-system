import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription } from '../ui/dialog';
import { Button } from '../ui/button';
import { RefreshCw, Radio } from 'lucide-react';

interface MountpointsModalProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  mountpoints: string[];
  onMountpointSelect: (mountpoint: string) => void;
  onRefresh: () => void;
  isFetching: boolean;
}

export function MountpointsModal({
  open,
  onOpenChange,
  mountpoints,
  onMountpointSelect,
  onRefresh,
  isFetching,
}: MountpointsModalProps) {
  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-md max-h-[80vh] flex flex-col">
        <DialogHeader>
          <DialogTitle>Available Mountpoints</DialogTitle>
          <DialogDescription>Choose a mountpoint from the NTRIP caster</DialogDescription>
        </DialogHeader>

        <div className="flex-1 overflow-y-auto space-y-1 min-h-0">
          {mountpoints.length === 0 ? (
            <div className="text-center py-8 text-muted-foreground">
              {isFetching ? 'Fetching...' : 'No mountpoints available'}
            </div>
          ) : (
            mountpoints.map((mp, idx) => (
              <button
                key={`${mp}-${idx}`}
                onClick={() => {
                  onMountpointSelect(mp);
                  onOpenChange(false);
                }}
                className="w-full flex items-center gap-3 p-3 rounded-lg hover:bg-accent text-left transition-colors"
              >
                <Radio className="size-5 flex-shrink-0" />
                <span className="font-medium">{mp}</span>
              </button>
            ))
          )}
        </div>

        <div className="flex-shrink-0 pt-4 border-t">
          <Button
            onClick={onRefresh}
            disabled={isFetching}
            variant="outline"
            className="w-full"
          >
            <RefreshCw className={`size-4 mr-2 ${isFetching ? 'animate-spin' : ''}`} />
            {isFetching ? 'Fetching...' : 'Refresh'}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  );
}
