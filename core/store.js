// healthStore：整個 App 唯一能讀寫資料的地方。未登入存在本機，登入後存 Firestore
import { firebaseConfig } from './firebase-config.js';
import { dateKey, goalFor } from './logic.js';
import { normalizeEvent, registry } from './registry.js';

const COLLECTIONS = ['events', 'goals', 'attachments', 'profile', 'trainings', 'schedules', 'exercises'];

class LocalBackend {
  #dbPromise = this.#open();

  #open() {
    return new Promise((resolve, reject) => {
      const request = indexedDB.open('health', 1);
      request.onupgradeneeded = () => request.result.createObjectStore('docs', { keyPath: 'key' });
      request.onsuccess = () => resolve(request.result);
      request.onerror = () => reject(request.error);
    });
  }

  async #transact(mode, fn) {
    const db = await this.#dbPromise;
    return new Promise((resolve, reject) => {
      const tx = db.transaction('docs', mode);
      const request = fn(tx.objectStore('docs'));
      tx.oncomplete = () => resolve(request.result);
      tx.onerror = () => reject(tx.error);
    });
  }

  // iPhone 切到背景後資料庫連線可能失效，失敗時重新開一次
  async #run(mode, fn) {
    try {
      return await this.#transact(mode, fn);
    } catch {
      this.#dbPromise = this.#open();
      return this.#transact(mode, fn);
    }
  }

  async list(col) {
    const range = IDBKeyRange.bound(`${col}/`, `${col}/￿`);
    const rows = await this.#run('readonly', (s) => s.getAll(range));
    return rows.map((r) => r.doc);
  }
  get(col, id) {
    return this.#run('readonly', (s) => s.get(`${col}/${id}`)).then((r) => r?.doc ?? null);
  }
  put(col, id, doc) {
    return this.#run('readwrite', (s) => s.put({ key: `${col}/${id}`, doc }));
  }
  delete(col, id) {
    return this.#run('readwrite', (s) => s.delete(`${col}/${id}`));
  }
}

class FirestoreBackend {
  constructor(db, uid) {
    this.root = db.collection('users').doc(uid);
  }
  async list(col) {
    const snap = await this.root.collection(col).get();
    return snap.docs.map((d) => d.data());
  }
  async get(col, id) {
    const snap = await this.root.collection(col).doc(id).get();
    return snap.exists ? snap.data() : null;
  }
  put(col, id, doc) {
    return this.root.collection(col).doc(id).set(doc);
  }
  delete(col, id) {
    return this.root.collection(col).doc(id).delete();
  }
}

class HealthStore extends EventTarget {
  user = null;
  events = new Map();
  goals = [];
  profile = {};
  trainings = [];
  schedules = [];
  exercises = [];
  #local = new LocalBackend();
  #backend = this.#local;
  #attachments = new Map();
  #db = null;
  #auth = null;

  async init() {
    await this.#load();
    if (!window.firebase) return;
    firebase.initializeApp(firebaseConfig);
    this.#db = firebase.firestore();
    this.#auth = firebase.auth();
    this.#auth.onAuthStateChanged((user) => this.#switchUser(user));
    window.addEventListener('online', () => this.reload());
  }

  async #switchUser(user) {
    if (user?.uid === this.user?.uid) return;
    this.user = user;
    if (user) {
      this.#backend = new FirestoreBackend(this.#db, user.uid);
      await this.#uploadLocalData();
      await this.#flushOutbox().catch(() => {});
    } else {
      this.#backend = this.#local;
    }
    this.#attachments.clear();
    await this.#load();
  }

  // 登入前記的資料，登入時搬到雲端；全部寫入成功才清除本機
  async #uploadLocalData() {
    try {
      for (const col of COLLECTIONS) {
        const docs = await this.#local.list(col);
        await Promise.all(docs.map((doc) => this.#backend.put(col, doc.id, doc)));
        await Promise.all(docs.map((doc) => this.#local.delete(col, doc.id)));
      }
    } catch (error) {
      this.#reportError(error);
    }
  }

  // 雲端讀不到時用上次的備份，再疊上還沒送出的修改，離線也能繼續用
  async #load() {
    const names = ['events', 'goals', 'profile', 'trainings', 'schedules', 'exercises'];
    let lists;
    try {
      lists = Object.fromEntries(await Promise.all(names.map(async (c) => [c, await this.#backend.list(c)])));
      if (this.user) await this.#local.put('mirror', this.user.uid, { id: this.user.uid, lists });
    } catch (error) {
      lists = this.user ? (await this.#local.get('mirror', this.user.uid).catch(() => null))?.lists : null;
      if (!lists) { this.#reportError(error); return; }
    }
    if (this.user) {
      for (const item of await this.#pendingWrites()) {
        lists[item.col] = [...lists[item.col].filter((doc) => doc.id !== item.doc.id), item.doc];
      }
    }
    this.events = new Map(lists.events.map((e) => [e.id, normalizeEvent(e)]));
    this.goals = lists.goals;
    this.profile = lists.profile[0] ?? {};
    this.trainings = lists.trainings;
    this.schedules = lists.schedules;
    this.exercises = lists.exercises;
    this.#changed();
  }

  async #pendingWrites() {
    const items = await this.#local.list('outbox').catch(() => []);
    return items.filter((item) => item.uid === this.user?.uid && item.col !== 'attachments');
  }

  // 重新連線後，把還沒送出的修改補送
  async #flushOutbox() {
    for (const item of await this.#pendingWrites()) {
      await this.#backend.put(item.col, item.doc.id, item.doc);
      await this.#local.delete('outbox', item.id);
    }
  }

  async reload() {
    if (this.user) await this.#flushOutbox().catch(() => {});
    return this.#load();
  }

  #changed() {
    this.dispatchEvent(new Event('change'));
  }

  #reportError(error) {
    console.error(error);
    this.dispatchEvent(new CustomEvent('error', { detail: error }));
  }

  // 先存到本機待送清單，雲端確認後才移除；網路或連線失敗時資料不會消失
  #write(col, id, doc) {
    const key = `${col}:${id}`;
    if (this.user) this.#local.put('outbox', key, { id: key, col, doc, uid: this.user.uid }).catch(() => {});
    try {
      Promise.resolve(this.#backend.put(col, id, doc))
        .then(() => (this.user ? this.#local.delete('outbox', key) : null))
        .catch((error) => this.#reportError(error));
    } catch (error) {
      this.#reportError(error);
    }
  }

  eventsOn(date) {
    return [...this.events.values()].filter((e) => e.date === date && !e.deletedAt).sort((a, b) => a.occurredAt - b.occurredAt);
  }

  liveEvents() {
    return [...this.events.values()].filter((e) => !e.deletedAt);
  }

  // 第一筆紀錄的日期；在這之前的日子不算沒做到
  firstRecordDate(eventTypes) {
    return this.liveEvents().filter((e) => eventTypes.includes(e.type)).reduce((min, e) => (!min || e.date < min ? e.date : min), null);
  }

  addEvent(type, data, { date, occurredAt: time, source = 'manual', raw = null, attachmentIds = [] } = {}) {
    if (!registry[type].validate(data)) throw new Error(`資料不完整：${type}`);
    const now = new Date();
    const today = dateKey(now);
    const eventDate = date ?? today;
    const occurredAt = time ?? (eventDate === today ? now.getTime() : new Date(`${eventDate}T12:00:00`).getTime());
    const event = {
      id: crypto.randomUUID(), type, schemaVersion: registry[type].version,
      occurredAt, tzOffset: -now.getTimezoneOffset(), date: eventDate,
      source, externalId: null, data, raw, attachmentIds,
      createdAt: now.getTime(), updatedAt: now.getTime(), deletedAt: null,
    };
    this.events.set(event.id, event);
    this.#write('events', event.id, event);
    this.#changed();
    return event;
  }

  updateEvent(id, data, patch = {}) {
    const current = this.events.get(id);
    const event = { ...current, ...patch, data: { ...current.data, ...data }, updatedAt: Date.now() };
    this.events.set(id, event);
    this.#write('events', id, event);
    this.#changed();
    return event;
  }

  deleteEvent(id) {
    const event = { ...this.events.get(id), deletedAt: Date.now() };
    this.events.set(id, event);
    this.#write('events', id, event);
    this.#changed();
  }

  restoreEvent(id) {
    const event = { ...this.events.get(id), deletedAt: null };
    this.events.set(id, event);
    this.#write('events', id, event);
    this.#changed();
  }

  // 目標只增不改：同一天的修改覆蓋當天那筆，歷史判定不受影響
  saveGoal(patch) {
    const effectiveFrom = dateKey();
    const doc = { ...goalFor(this.goals, effectiveFrom), ...patch, id: effectiveFrom, effectiveFrom, updatedAt: Date.now() };
    this.goals = [...this.goals.filter((g) => g.id !== doc.id), doc];
    this.#write('goals', doc.id, doc);
    this.#changed();
  }

  // 訓練和排程共用同一種存法，刪除只做標記
  saveDoc(col, doc) {
    const saved = { ...doc, id: doc.id ?? crypto.randomUUID(), updatedAt: Date.now() };
    this[col] = [...this[col].filter((d) => d.id !== saved.id), saved];
    this.#write(col, saved.id, saved);
    this.#changed();
    return saved;
  }

  activeDocs(col) {
    return this[col].filter((d) => !d.deletedAt);
  }

  saveProfile(patch) {
    this.profile = { ...this.profile, ...patch, id: 'main', updatedAt: Date.now() };
    this.#write('profile', 'main', this.profile);
    this.#changed();
  }

  async getAttachment(id) {
    if (!this.#attachments.has(id)) {
      const doc = await this.#backend.get('attachments', id);
      if (doc) this.#attachments.set(id, doc);
    }
    return this.#attachments.get(id) ?? null;
  }

  async exportAll() {
    const lists = await Promise.all(COLLECTIONS.map((c) => this.#backend.list(c)));
    return { exportedAt: new Date().toISOString(), ...Object.fromEntries(COLLECTIONS.map((c, i) => [c, lists[i]])) };
  }

  signIn() {
    return this.#auth.signInWithPopup(new firebase.auth.GoogleAuthProvider());
  }
  signOut() {
    return this.#auth.signOut();
  }
  get canSignIn() {
    return !!this.#auth;
  }
}

export const healthStore = new HealthStore();
