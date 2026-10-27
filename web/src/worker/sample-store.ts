// D-28: the samples a visitor trains, kept in IndexedDB (the browser's built-in database) so
// they survive a refresh. Stays on their device; nothing is uploaded.

export const STORE_CAP = 50;

export interface StoredSample {
  id?: number;
  label: string;
  features: number[];
  embedding: Float32Array | null; // null if added before ResNet18 had loaded
  thumb: Blob | null; // the crop it learned from
  createdAt: number;
}

const DB_NAME = "silhouette";
const STORE = "samples";

function request<T>(r: IDBRequest<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    r.onsuccess = () => resolve(r.result);
    r.onerror = () => reject(r.error);
  });
}

export class SampleStore {
  private constructor(private db: IDBDatabase) {}

  // Null when IndexedDB is unavailable (some private-browsing modes): samples then live only
  // as long as the tab.
  static async open(): Promise<SampleStore | null> {
    try {
      const open = indexedDB.open(DB_NAME, 1);
      open.onupgradeneeded = () =>
        open.result.createObjectStore(STORE, { keyPath: "id", autoIncrement: true });
      return new SampleStore(await request(open));
    } catch {
      return null;
    }
  }

  private store(mode: IDBTransactionMode) {
    return this.db.transaction(STORE, mode).objectStore(STORE);
  }

  all(): Promise<StoredSample[]> {
    return request(this.store("readonly").getAll() as IDBRequest<StoredSample[]>);
  }

  count(): Promise<number> {
    return request(this.store("readonly").count());
  }

  async add(sample: StoredSample): Promise<void> {
    await request(this.store("readwrite").add(sample));
  }

  async remove(id: number): Promise<void> {
    await request(this.store("readwrite").delete(id));
  }

  async removeLabel(label: string): Promise<void> {
    const store = this.store("readwrite");
    const all = await request(store.getAll() as IDBRequest<StoredSample[]>);
    await Promise.all(all.filter((s) => s.label === label).map((s) => request(store.delete(s.id!))));
  }

  async clear(): Promise<void> {
    await request(this.store("readwrite").clear());
  }
}
